#include "TemplateManager.h"
#include <algorithm>
#include <cmath>
#include <cstdint> // 提供 int64_t/uint32_t 等定长整数类型，用于稳定打包哈希键
#include <limits>
#include <unordered_map> // 用于构建二维网格直方图（确定性选峰），替代 rand 随机抽样
#include <exception>
#include <opencv2/imgproc.hpp>
#include <bscv/templMatchTypes.h>
#include <QDebug> // 用于输出掩膜尺寸异常日志
#include <QString>

namespace { // 工具函数
cv::Mat toGrayMat(const cv::Mat& image) 
{
    if (image.empty()) { 
        return cv::Mat(); 
    }
    if (image.channels() == 1) { 
        return image.clone(); 
    } 
    cv::Mat gray; 
    int code = image.channels() == 3 ? cv::COLOR_BGR2GRAY : cv::COLOR_BGRA2GRAY;
    cv::cvtColor(image, gray, code); 
    return gray;
} 

cv::Rect buildResultRect(const TIGER_BSVISION::Match& match, const cv::Point& offset, const cv::Size& bounds) // 将 bscv 匹配结果转换成全图坐标矩形
{ 
    int width = std::max(1, match.width); 
    int height = std::max(1, match.height); 
    cv::Rect rect(match.x + offset.x, match.y + offset.y, width, height); // 叠加 ROI 偏移得到全图坐标
    cv::Rect imageRect(0, 0, bounds.width, bounds.height); // 构建整图矩形用于裁剪
    return rect & imageRect;
}

double normalizeScore(double similarity) 
{
    return std::clamp(similarity / 100.0, 0.0, 1.0);
} 
// 根据中心距离抑制重复的匹配结果，避免同一位置被反复标记
bool isDuplicateMatch(const MatchResult& candidate, const std::vector<MatchResult>& accepted, double distanceThreshold)
{
    for (const auto& existing : accepted) {
        const double dist = cv::norm(candidate.center - existing.center);
        const double angleDelta = std::abs(candidate.angle - existing.angle);
        if (dist <= distanceThreshold) {
            return true;
        }
    }
    return false;
}

}// namespace

TemplateManager::TemplateManager()
{
    m_matchEngine.reset(TIGER_BSVISION::newTemplMatch());
}
TemplateManager::~TemplateManager()
{
    if (m_matchEngine) {
        m_matchEngine->clear();
        m_matchEngine.reset();
    }
}
bool TemplateManager::learnTemplate(const cv::Mat& src, const cv::Mat& templateMat,const MatchParams& params) 
{
    if (src.empty() || templateMat.empty())
    {
        return false;
    }

    cv::Mat roiMat = templateMat.clone();
    cv::Mat grayTemplate = toGrayMat(roiMat);
    if (grayTemplate.empty())
    {
        return false;
    }
    if (!m_matchEngine) 
    {
        m_matchEngine.reset(TIGER_BSVISION::newTemplMatch()); // 通过 bscv 工厂创建模板匹配实例
    }
    if (!m_matchEngine) 
    {
        return false;
    }
    m_matchEngine->clear(); // 清空旧模板，确保新的学习参数生效
    float angleStart = 0.0f;
    // 根据参数构造模板学习掩膜：屏蔽距离边界较近的像素，避免把 ROI 边框当作特征
    cv::Mat templateMask; 

    // 如果输入是4通道图像（带Alpha），则使用Alpha通道作为掩膜，并裁剪到最小外接矩形
    if (roiMat.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(roiMat, channels);
        cv::Mat alpha = channels[3];
        
        // 计算非零区域的外接矩形
        cv::Rect bbox = cv::boundingRect(alpha);
        if (bbox.area() > 0) {
            // 裁剪图像和掩膜到外接矩形
            grayTemplate = grayTemplate(bbox).clone();
            templateMask = alpha(bbox).clone();
            roiMat = roiMat(bbox).clone(); // 更新 roiMat 以便后续保存正确的模板图像
        }
    }
    else if (params.borderPadding > 0) {
        const int pad = std::max(0, std::min(params.borderPadding, std::min(grayTemplate.cols, grayTemplate.rows) / 2));
        templateMask = cv::Mat(grayTemplate.size(), CV_8UC1, cv::Scalar(0));
        const int innerW = std::max(0, grayTemplate.cols - 2 * pad);
        const int innerH = std::max(0, grayTemplate.rows - 2 * pad);
        if (innerW > 0 && innerH > 0) {
            cv::Rect inner(pad, pad, innerW, innerH);
            cv::rectangle(templateMask, inner, cv::Scalar(255), cv::FILLED);
        }
    }
    TIGER_BSVISION::CPolarityType _type;
    if( params.ignorePolarity ){
        _type = TIGER_BSVISION::cptIgnore;
    }else{
        _type = TIGER_BSVISION::cptSame;
    }
    bool ok = m_matchEngine->create_shape_model(grayTemplate, templateMask, angleStart, params.angleRange,
                                                params.angle_step / 2.0f, params.scale_min, params.scale_max, params.scale_step, 
                                                params.weakThreshold, params.strongThreshold, params.FeaturePointNum, _type); 
    if (!ok) {
        m_valid = false; // 标记当前模板失效
        m_featurePoints.clear();
        return false;
    }
    m_featurePoints.clear();
    m_template = roiMat; // 保存原始 ROI 图像供其它模块显示

    m_learnParams = params; // 缓存学习时使用的参数
   
    m_valid = true; // 标记已有有效模板
    m_featurePoints = collectFeaturePoints(m_matchEngine->getTempl(0));
    cv::RotatedRect rr = cv::minAreaRect(m_featurePoints);
    m_trainCenter = rr.center; // 默认训练中心为 ROI 几何中心
    m_featureBounds = computeFeatureBounds(m_featurePoints);
    if (m_featureBounds.width <= 0.f || m_featureBounds.height <= 0.f) {
        m_featureBounds = cv::Rect2f(0.f, 0.f,
                                     static_cast<float>(roiMat.cols),
                                     static_cast<float>(roiMat.rows));
    }
    return true;
}

std::vector<MatchResult> TemplateManager::matchTemplate(const cv::Mat& src, const FindMatchParams& params) const 
{
    std::vector<MatchResult> results; // 结果集合
    if (!m_valid || !m_matchEngine || src.empty()) 
    {
        return results;
    }
    cv::Size imageSize(src.cols, src.rows); // 记录整图尺寸以便裁剪
    cv::Rect region(0, 0, imageSize.width, imageSize.height);
    if (region.width <= 0 || region.height <= 0) 
    {
        return results;
    }

    cv::Mat graySearch = toGrayMat(src); // 将源图像转为灰度图用于匹配
    if (graySearch.empty()) 
    {
        return results;
    }

    cv::Mat maskParam = params.Mask; // 从参数中获取掩膜
    // 修改：如果掩膜非空，确保其为单通道格式
    if (!maskParam.empty() && maskParam.type() != CV_8UC1) {
        if (maskParam.channels() > 1) {
            cv::cvtColor(maskParam, maskParam, cv::COLOR_BGR2GRAY); // 多通道掩膜转为灰度
        } else {
            maskParam.convertTo(maskParam, CV_8U); // 确保数据类型为 8 位无符号整型
        }
    }
    if (!maskParam.empty()) {
        const bool sizeMismatch = maskParam.cols != graySearch.cols || maskParam.rows != graySearch.rows;
        if (sizeMismatch) {
            qWarning().noquote() << QStringLiteral("匹配掩膜尺寸与输入图像不一致，已忽略掩膜：mask=%1x%2 image=%3x%4")
                                    .arg(maskParam.cols)
                                    .arg(maskParam.rows)
                                    .arg(graySearch.cols)
                                    .arg(graySearch.rows);
            maskParam.release(); // 丢弃失配的掩膜，避免后续 Mat ROI 断言（中文提示）
        }
    }

    cv::Mat MatchImg;
    if (params.useRoi && !maskParam.empty()) {
        cv::bitwise_and(graySearch, graySearch, MatchImg, maskParam); // 使用掩码提取目标区域
        maskParam.release(); //  第三方匹配库在启用掩膜时会做额外 ROI 处理，这里直接清空掩膜，改为依赖零化后的图像避免再次触发 ROI 越界
    } else {
        MatchImg = graySearch.clone();
    }

    std::vector<TIGER_BSVISION::Match> matches;
    try {
        matches = m_matchEngine->find_shape_model(MatchImg, maskParam, static_cast<float>(params.scoreThreshold));
    } catch (const cv::Exception& e) {
        qWarning().noquote() << QStringLiteral("find_shape_model 内部抛出 OpenCV 异常，已终止匹配：%1").arg(QString::fromUtf8(e.what()));
        return results; //  拦截第三方库的 ROI 异常，避免进程直接终止
    } catch (const std::exception& e) {
        qWarning().noquote() << QStringLiteral("find_shape_model 内部抛出异常，已终止匹配：%1").arg(QString::fromUtf8(e.what()));
        return results;
    } catch (...) {
        qWarning().noquote() << QStringLiteral("find_shape_model 内部出现未知异常，已终止匹配");
        return results;
    }
    std::sort(matches.begin(), matches.end(), [](const TIGER_BSVISION::Match& lhs,
              const TIGER_BSVISION::Match& rhs) { return lhs.similarity > rhs.similarity; }); // 按相似度降序排列
    int maxCount = std::max(1, params.maxCount); //至少返回一个结果

    cv::Point offset(0, 0); // 坐标偏移量为零,直接在全图上匹配
    results.reserve(std::min(static_cast<size_t>(maxCount), matches.size()));
    cv::Point2f Matchcenter;
    Matchcenter = m_trainCenter;
    for (const auto& match : matches) {
        cv::Rect rect = buildResultRect(match, offset, imageSize); // 将匹配框转换到整图坐标
        if (rect.width <= 0 || rect.height <= 0) {
            continue;
        }
        MatchResult item; // 构建界面需要的结果结构
        if (match.pts.empty()) {continue;}

        item.transformedFeaturePoints = match.pts; 


        item.score = normalizeScore(match.similarity); // 将相似度归一化到 0~1 以呼应 m_matchScoreSpinBox 的展示逻辑
        item.angle = match.angle;
        item.scale = match.scale;

        item.center = match.transPt(cv::Point2f((Matchcenter))); // 计算中心点坐标
        item.Roiarea = rect = boundingRect(item.transformedFeaturePoints);
        cv::RotatedRect rr = cv::minAreaRect(item.transformedFeaturePoints);
        item.featureSize = rr.size; 
        item.rotatedRect = rr;
        /*
        // if (!match.pts.empty()) {
        //     item.transformedFeaturePoints = match.pts; 
        //     if (!item.transformedFeaturePoints.empty()) {
        //         // 计算最小外接矩形，用于确定中心和尺寸
        //         cv::RotatedRect rr = cv::minAreaRect(item.transformedFeaturePoints);
                
        //         //bool fixedCenterFound = false;

        //         //if (params.centerType == MatchCenterType::SceneCenter) {
        //             //qDebug() <<"原始点："<< m_featurePoints.size() << "识别点" << item.transformedFeaturePoints.size();
        //             // if (m_featurePoints.size() == item.transformedFeaturePoints.size() && !m_featurePoints.empty()) {
        //             //      cv::Mat inliers;
        //             //      cv::Mat affine = cv::estimateAffinePartial2D(m_featurePoints, item.transformedFeaturePoints, inliers);
        //             //     //  if (!affine.empty()) {
        //             //     //      std::vector<cv::Point2f> srcObj = { m_trainCenter };
        //             //     //      std::vector<cv::Point2f> dstObj;
        //             //     //      cv::transform(srcObj, dstObj, affine);
        //             //     //      item.center = dstObj[0];
        //             //     //      fixedCenterFound = true;
        //             //     //  }
        //             // }

        //                 // 2. 点数不一致，使用改进的“RANSAC”对齐
        //             //if (!fixedCenterFound && !m_featurePoints.empty() && !item.transformedFeaturePoints.empty()) {
                        
        //                 // double rawAngle = item.angle;
        //                 // double s = item.scale;

        //                 // // 计算模型重心
        //                 // cv::Point2f modelCentroid(0, 0);
        //                 // for(const auto& p : m_featurePoints) modelCentroid += p;
        //                 // modelCentroid *= (1.0 / m_featurePoints.size());

        //                 // // 计算结果重心 (作为平移的粗略参考)
        //                 // cv::Point2f sceneCentroid(0, 0);
        //                 // for(const auto& p : item.transformedFeaturePoints) sceneCentroid += p;
        //                 // sceneCentroid *= (1.0 / item.transformedFeaturePoints.size());

        //                 // // 定义尝试的策略：尝试正角度和负角度 (应对坐标系定义差异)
        //                 // // 有些库逆时针为正，有些顺时针为正
        //                 // struct CandidateStrategy {
        //                 //     double angle;
        //                 //     std::vector<cv::Point2f> pairedModel;
        //                 //     std::vector<cv::Point2f> pairedScene;
        //                 //     int inliersCount;
        //                 // };

        //                 // CandidateStrategy strategies[2];
        //                 // strategies[0].angle = rawAngle;
        //                 // strategies[1].angle = -rawAngle;

        //                 // for (int k = 0; k < 2; ++k) {
        //                 //     strategies[k].inliersCount = 0;
        //                 //     double rad = strategies[k].angle * CV_PI / 180.0;
        //                 //     double cosA = std::cos(rad);
        //                 //     double sinA = std::sin(rad);

        //                     // 预计算旋转并缩放后的模型点 (相对于模型重心)
        //                     // std::vector<cv::Point2f> transformedModelPts;
        //                     // transformedModelPts.reserve(m_featurePoints.size());
        //                     // for(const auto& p : m_featurePoints) {
        //                     //     float dx = p.x - modelCentroid.x;
        //                     //     float dy = p.y - modelCentroid.y;
        //                     //     transformedModelPts.emplace_back((dx * cosA - dy * sinA) * s, (dx * sinA + dy * cosA) * s);
        //                     // }

        //                     // 寻找最佳平移量 (位移投票)
        //                     // 由于点数差异，寻找众数平移量
        //                     // std::vector<cv::Point2f> votes; // 存放每个模型采样点与每个场景点之间的位移向量（投票）
        //                     // votes.reserve(m_featurePoints.size());
        //                     // 简化投票：只用模型点去“探测”结果点
        //                     // 取部分模型点采样
        //                     // int step = std::max(1, (int)m_featurePoints.size() / 60);
        //                     // for (size_t i = 0; i < m_featurePoints.size(); i += step) {
        //                     //     for (const auto& sp : item.transformedFeaturePoints) { 
        //                     //         votes.push_back(sp - transformedModelPts[i]);
        //                     //     }
        //                     // }

        //                     //// 寻找投票密集的区域
        //                     //// 使用简化的网格法找峰值
        //                     // if (votes.empty()) continue;

        //                     // // 寻找最密集的平移向量
        //                     // cv::Point2f bestTranslation = sceneCentroid - modelCentroid; 
        //                     // int maxConsensus = 0;
        //                     // const float distThresh = 20.0f * (float)s; 

        //                     // // 采用确定性的二维网格直方图找“众数平移峰值”，避免 rand() 导致同图多次点击结果不稳定
        //                     // struct BinAccum {
        //                     //     int count = 0; // 该 bin 内落入的投票数量（共识数量）
        //                     //     cv::Point2f sum = {0.f, 0.f}; // 该 bin 内所有投票向量的累加和（用于求均值）
        //                     // };

        //                     // auto packKey = [](int bx, int by) -> std::int64_t { // 将二维 bin 坐标打包为 64 位 key 作为哈希键
        //                     //     return (static_cast<std::int64_t>(bx) << 32) ^ (static_cast<std::uint32_t>(by));
        //                     // };

        //                     // const float binSize = std::max(1.0f, distThresh * 0.5f); // 网格尺寸：越小峰越尖锐，但过小会碎峰；这里取 distThresh 的一半
        //                     // const float invBin = 1.0f / binSize; // 预计算 1/binSize，减少循环内除法开销
        //                     // const cv::Point2f roughTranslation = sceneCentroid - modelCentroid; // 粗略平移先验：用于峰值并列时的确定性裁决（tie-break）

        //                     // std::unordered_map<std::int64_t, BinAccum> hist; // 二维网格直方图：key->(count,sum)
        //                     // hist.reserve(votes.size() * 2); // 预留容量减少 rehash，提升稳定性与性能

        //                     // for (const auto& v : votes) { // 遍历所有投票平移向量
        //                     //     const int bx = static_cast<int>(std::lround(v.x * invBin)); // 将 x 分量量化到网格坐标（用 round 减少边界抖动）
        //                     //     const int by = static_cast<int>(std::lround(v.y * invBin)); // 将 y 分量量化到网格坐标（用 round 减少边界抖动）
        //                     //     auto& acc = hist[packKey(bx, by)]; // 找到对应 bin 的累加器（不存在则创建）
        //                     //     acc.count++; // 该 bin 共识计数 +1
        //                     //     acc.sum += v; // 累加该 bin 的投票向量和，用于后续求均值
        //                     // } // 结束 votes 遍历

        //                     // int bestCount = -1; // 当前找到的最大 bin 共识数（用于选峰）
        //                     // cv::Point2f bestMean = bestTranslation; // 当前找到的最佳平移（初始化为粗略平移兜底）

        //                     // for (const auto& kv : hist) { // 遍历所有 bin，选出共识最多的峰
        //                     //     const BinAccum& acc = kv.second; // 取出该 bin 的累加器
        //                     //     if (acc.count <= 0) { // 理论上不会出现，但防御性判断更稳妥
        //                     //         continue;
        //                     //     }
        //                     //     const cv::Point2f mean = acc.sum * (1.0f / acc.count); // 该 bin 内投票向量均值，作为该峰的平移估计
        //                     //     // if (acc.count > bestCount) { // 若该峰共识更多
        //                     //     //     bestCount = acc.count; // 更新最大共识数
        //                     //     //     bestMean = mean; // 更新最佳平移为该峰均值（更平滑、更稳）
        //                     //     // } else if (acc.count == bestCount) { // 若出现并列峰（多个峰共识相同）
        //                     //     //     if (cv::norm(mean - roughTranslation) < cv::norm(bestMean - roughTranslation)) { // 选更接近粗略平移的峰，确保确定性且贴近先验
        //                     //     //         bestMean = mean; // 用更合理的并列峰替换当前最佳
        //                     //     //     }
        //                     //     // }
        //                     // }

        //                     // bestTranslation = bestMean; // 用确定性选出的峰值平移替代随机 seed
        //                     // maxConsensus = std::max(0, bestCount);

        //                     // 基于最佳平移量建立对应关系
        //                     // for(size_t i=0; i<m_featurePoints.size(); ++i) {
        //                     //     cv::Point2f predicted = transformedModelPts[i] + bestTranslation;
                                
        //                     //     int bestIdx = -1;
        //                     //     double minDistSq = 1e9;
        //                     //     // for(size_t j=0; j<item.transformedFeaturePoints.size(); ++j) {
        //                     //     //     double dx = item.transformedFeaturePoints[j].x - predicted.x;
        //                     //     //     double dy = item.transformedFeaturePoints[j].y - predicted.y;
        //                     //     //     double dSq = dx*dx + dy*dy;
        //                     //     //     if(dSq < minDistSq) {
        //                     //     //         minDistSq = dSq;
        //                     //     //         bestIdx = (int)j;
        //                     //     //     }
        //                     //     // }

        //                     //     // if(bestIdx != -1 && minDistSq < (distThresh*distThresh)) {
        //                     //     //     strategies[k].pairedModel.push_back(m_featurePoints[i]); 
        //                     //     //     strategies[k].pairedScene.push_back(item.transformedFeaturePoints[bestIdx]);
        //                     //     //     strategies[k].inliersCount++;
        //                     //     // }
        //                     // }
        //                 //}

        //                 // 比较哪种策略(正角还是负角)找到了更多的匹配点
        //                 //int bestStratIdx = (strategies[0].inliersCount >= strategies[1].inliersCount) ? 0 : 1;
        //                 //auto& bestStrat = strategies[bestStratIdx];

        //                 // 使用最好的匹配集进行最终的变换矩阵计算
        //                 // if (bestStrat.inliersCount >= 4) {
        //                 //     cv::Mat inliers; // 用于接收 RANSAC 的内点掩码（如果使用）
        //                 //     cv::Mat affine = cv::estimateAffinePartial2D(bestStrat.pairedModel, bestStrat.pairedScene, inliers); // 估计部分仿射变换（平移+旋转+缩放）
        //                 //     // if (!affine.empty()) {
        //                 //     //     std::vector<cv::Point2f> srcObj = { m_trainCenter };
        //                 //     //     std::vector<cv::Point2f> dstObj;
        //                 //     //     cv::transform(srcObj, dstObj, affine);
        //                 //     //     item.center = dstObj[0];
        //                 //     //     fixedCenterFound = true;
        //                 //     // }
        //                 // }
        //             //}
        //         //} 
                
        //         // // 3. 如果所有高级方法都失败，回退到最小外接矩形中心
        //         // if (!fixedCenterFound) {
        //         //     item.center = rr.center; 
        //         // }

        //         item.featureSize = rr.size; 
        //         item.rotatedRect = rr;
                
        //         //计算轴对齐包围盒（ROI区域），并裁剪到图像范围内
        //         cv::Point2f boxPts[4];
        //         rr.points(boxPts);
        //         std::vector<cv::Point2f> corners(boxPts, boxPts + 4);
        //         cv::Rect axisAligned = cv::boundingRect(corners);
        //         cv::Rect imageRect(0, 0, imageSize.width, imageSize.height);
        //         axisAligned &= imageRect; 
        //         if (axisAligned.width > 0 && axisAligned.height > 0) {
        //             item.Roiarea = axisAligned;
        //         }
        //     }
        // }
        */

        const double diag = std::hypot(static_cast<double>(item.featureSize.width),
                                       static_cast<double>(item.featureSize.height));
        const double distanceThreshold = std::max(10.0, diag * 0.3);
        if (isDuplicateMatch(item, results, distanceThreshold)) {
            continue;
        }
        results.push_back(item);
        if (static_cast<int>(results.size()) >= maxCount) {
            break;
        }
    }
    return results;
} 

std::vector<cv::Point2f> TemplateManager::currentFeaturePoints() const
{
    return m_featurePoints;
}

std::vector<cv::Point2f> TemplateManager::collectFeaturePoints(const TIGER_BSVISION::Template& templ)
{
    std::vector<cv::Point2f> points;
    points.reserve(templ.features.size());
    const float offsetX = static_cast<float>(templ.tl_x);
    const float offsetY = static_cast<float>(templ.tl_y);    
    for (const auto& feature : templ.features) {
        points.emplace_back(static_cast<float>(feature.x + offsetX),
                    static_cast<float>(feature.y + offsetY)); // 转换成以 ROI 左上角为原点的图像坐标
    }
    return points;
}
    // 从特征点集合中计算最小外接矩形，后续绘制用真实模板尺寸
cv::Rect2f TemplateManager::computeFeatureBounds(const std::vector<cv::Point2f>& points)
{
    if (points.empty()) {
        return cv::Rect2f();
    }
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& pt : points) {
        minX = std::min(minX, pt.x);
        minY = std::min(minY, pt.y);
        maxX = std::max(maxX, pt.x);
        maxY = std::max(maxY, pt.y);
    }
    return cv::Rect2f(minX,
                      minY,
                      std::max(0.0f, maxX - minX),
                      std::max(0.0f, maxY - minY));
}



