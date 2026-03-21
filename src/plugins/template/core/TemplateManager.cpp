#include "TemplateManager.h"
#include <algorithm>
#include <cmath>
#include <cstdint> // 提供 int64_t/uint32_t 等定长整数类型，用于稳定打包哈希键
#include <limits>
#include <set>
#include <unordered_map> // 用于构建二维网格直方图（确定性选峰），替代 rand 随机抽样
#include <exception>
#include <opencv2/imgproc.hpp>
#include <bscv/templMatchTypes.h>
#include <QDebug> // 用于输出掩膜尺寸异常日志
#include <QString>

namespace { // 工具函数
constexpr int kMaxPyramidScanLevel = 3;

int calcValidPyramidLevel(const cv::Size& size, int requestedLevel)
{
    int level = std::max(0, requestedLevel);
    int maxLevel = 0;
    int minSide = std::min(size.width, size.height);
    // pyrDown 每次尺寸约减半，最小边到 1 前都可继续降采样。
    while (minSide >= 2) {
        ++maxLevel;
        minSide /= 2;
    }
    return std::min(level, maxLevel);
}

cv::Mat pyramidDown(const cv::Mat& image, int level)
{
    if (image.empty() || level <= 0) {
        return image.clone();
    }
    cv::Mat current = image.clone();
    for (int i = 0; i < level; ++i) {
        if (current.cols < 2 || current.rows < 2) {
            break;
        }
        cv::Mat next;
        cv::pyrDown(current, next);
        current = next;
    }
    return current;
}

cv::Rect scaleRectUp(const cv::Rect& rect, float scale, const cv::Size& bounds)
{
    if (rect.width <= 0 || rect.height <= 0) {
        return cv::Rect();
    }
    const int x = static_cast<int>(std::round(rect.x * scale));
    const int y = static_cast<int>(std::round(rect.y * scale));
    const int w = std::max(1, static_cast<int>(std::round(rect.width * scale)));
    const int h = std::max(1, static_cast<int>(std::round(rect.height * scale)));
    cv::Rect up(x, y, w, h);
    return up & cv::Rect(0, 0, bounds.width, bounds.height);
}

std::vector<cv::Point2f> scalePointsUp(const std::vector<cv::Point2f>& pts, float scale)
{
    if (scale == 1.0f) {
        return pts;
    }
    std::vector<cv::Point2f> out;
    out.reserve(pts.size());
    for (const auto& p : pts) {
        out.emplace_back(p.x * scale, p.y * scale);
    }
    return out;
}

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
}
TemplateManager::~TemplateManager()
{
    for (auto& engine : m_matchEngines) {
        if (engine) {
            engine->clear();
            engine.reset();
        }
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
    for (auto& engine : m_matchEngines) {
        if (engine) {
            engine->clear();
            engine.reset();
        }
    }
    m_effectiveLevels.fill(0);
    m_engineTrainCenters.fill(cv::Point2f(0.f, 0.f));
    m_valid = false;
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
    m_pyramidLevel = calcValidPyramidLevel(grayTemplate.size(), params.compressionLevel);
    m_pyramidScale = static_cast<float>(1 << m_pyramidLevel);
    const int scanUpperLevel = std::min(m_pyramidLevel, kMaxPyramidScanLevel);
    m_featurePoints.clear();
    m_template = roiMat; // 保存原始 ROI 图像供其它模块显示

    m_learnParams = params; // 缓存学习时使用的参数
    std::set<int> builtLevels;
    bool level0Captured = false;
    for (int requestLevel = 0; requestLevel <= scanUpperLevel; ++requestLevel) {
        const int effectiveLevel = calcValidPyramidLevel(grayTemplate.size(), requestLevel);
        m_effectiveLevels[requestLevel] = effectiveLevel;
        if (builtLevels.find(effectiveLevel) != builtLevels.end()) {
            continue;
        }

        std::unique_ptr<TIGER_BSVISION::ITemplMatch> engine(TIGER_BSVISION::newTemplMatch());
        if (!engine) {
            continue;
        }

        cv::Mat engineTemplate = pyramidDown(grayTemplate, effectiveLevel);
        cv::Mat engineMask;
        if (!templateMask.empty()) {
            engineMask = pyramidDown(templateMask, effectiveLevel);
            cv::threshold(engineMask, engineMask, 1, 255, cv::THRESH_BINARY);
        }

        const bool ok = engine->create_shape_model(engineTemplate,
                                                   engineMask,
                                                   angleStart,
                                                   params.angleRange,
                                                   params.angle_step / 2.0f,
                                                   params.scale_min,
                                                   params.scale_max,
                                                   params.scale_step,
                                                   params.weakThreshold,
                                                   params.strongThreshold,
                                                   params.FeaturePointNum,
                                                   _type);
        if (!ok) {
            continue;
        }

        const std::vector<cv::Point2f> engineFeaturePoints = collectFeaturePoints(engine->getTempl(0));
        cv::RotatedRect rr = cv::minAreaRect(engineFeaturePoints.empty() ? std::vector<cv::Point2f>{cv::Point2f(0.f, 0.f)} : engineFeaturePoints);
        const cv::Point2f engineCenter = rr.center;

        // 可能有多个 requestLevel 映射到同一个 effectiveLevel，把槽位都复用到这个引擎配置。
        for (size_t slot = 0; slot < m_effectiveLevels.size(); ++slot) {
            if (m_effectiveLevels[slot] == effectiveLevel) {
                m_engineTrainCenters[slot] = engineCenter;
            }
        }

        if (!level0Captured && effectiveLevel == 0) {
            m_featurePoints = engineFeaturePoints;
            m_engineTrainCenter = engineCenter;
            m_trainCenter = engineCenter;
            level0Captured = true;
        }

        builtLevels.insert(effectiveLevel);
        // 为该 effectiveLevel 找到一个对应槽位保存引擎。
        for (size_t slot = 0; slot < m_effectiveLevels.size(); ++slot) {
            if (m_effectiveLevels[slot] == effectiveLevel && !m_matchEngines[slot]) {
                m_matchEngines[slot] = std::move(engine);
                break;
            }
        }
    }

    if (!level0Captured) {
        // 兜底：使用第一个成功引擎的信息作为对外中心/特征点。
        for (size_t slot = 0; slot < m_matchEngines.size(); ++slot) {
            if (!m_matchEngines[slot]) {
                continue;
            }
            const std::vector<cv::Point2f> engineFeaturePoints = collectFeaturePoints(m_matchEngines[slot]->getTempl(0));
            const float scale = static_cast<float>(1 << m_effectiveLevels[slot]);
            m_featurePoints = scalePointsUp(engineFeaturePoints, scale);
            m_engineTrainCenter = m_engineTrainCenters[slot];
            m_trainCenter = cv::Point2f(m_engineTrainCenter.x * scale, m_engineTrainCenter.y * scale);
            break;
        }
    }

    m_valid = std::any_of(m_matchEngines.begin(), m_matchEngines.end(),
                          [](const std::unique_ptr<TIGER_BSVISION::ITemplMatch>& ptr) { return static_cast<bool>(ptr); });
    if (!m_valid) {
        m_featurePoints.clear();
        return false;
    }

    if (level0Captured) {
        m_featurePoints = scalePointsUp(m_featurePoints, 1.0f);
    }

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
    if (!m_valid || src.empty()) 
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

    std::set<int> executedLevels;
    int maxCount = std::max(1, params.maxCount); //至少返回一个结果
    const int scanUpperLevel = std::min(m_pyramidLevel, kMaxPyramidScanLevel);

    cv::Point offset(0, 0); // 坐标偏移量为零,直接在全图上匹配
    for (size_t slot = 0; slot < m_matchEngines.size(); ++slot) {
        if (!m_matchEngines[slot]) {
            continue;
        }
        if (m_effectiveLevels[slot] > scanUpperLevel) {
            continue;
        }

        const int level = calcValidPyramidLevel(graySearch.size(), m_effectiveLevels[slot]);
        if (!executedLevels.insert(level).second) {
            continue; // 有效层级重复时只跑一次
        }

        const float scaleToOriginal = static_cast<float>(1 << level);
        cv::Mat searchForEngine = pyramidDown(graySearch, level);
        cv::Mat levelMask = maskParam;
        if (!levelMask.empty()) {
            levelMask = pyramidDown(levelMask, level);
            cv::threshold(levelMask, levelMask, 1, 255, cv::THRESH_BINARY);
        }

        cv::Mat MatchImg;
        if (params.useRoi && !levelMask.empty()) {
            cv::bitwise_and(searchForEngine, searchForEngine, MatchImg, levelMask); // 使用掩码提取目标区域
            levelMask.release(); // 第三方匹配库在启用掩膜时会做额外 ROI 处理，这里直接清空掩膜避免越界
        } else {
            MatchImg = searchForEngine.clone();
        }

        std::vector<TIGER_BSVISION::Match> matches;
        try {
            matches = m_matchEngines[slot]->find_shape_model(MatchImg, levelMask, static_cast<float>(params.scoreThreshold));
        } catch (const cv::Exception& e) {
            qWarning().noquote() << QStringLiteral("find_shape_model 在金字塔层 %1 抛出 OpenCV 异常：%2")
                                    .arg(level)
                                    .arg(QString::fromUtf8(e.what()));
            continue;
        } catch (const std::exception& e) {
            qWarning().noquote() << QStringLiteral("find_shape_model 在金字塔层 %1 抛出异常：%2")
                                    .arg(level)
                                    .arg(QString::fromUtf8(e.what()));
            continue;
        } catch (...) {
            qWarning().noquote() << QStringLiteral("find_shape_model 在金字塔层 %1 出现未知异常").arg(level);
            continue;
        }

        std::sort(matches.begin(), matches.end(), [](const TIGER_BSVISION::Match& lhs,
                  const TIGER_BSVISION::Match& rhs) { return lhs.similarity > rhs.similarity; });

        const cv::Point2f matchCenter = m_engineTrainCenters[slot];
        for (const auto& match : matches) {
            cv::Rect rect = buildResultRect(match, offset, MatchImg.size());
            rect = scaleRectUp(rect, scaleToOriginal, imageSize);
            if (rect.width <= 0 || rect.height <= 0) {
                continue;
            }

            MatchResult item;
            item.transformedFeaturePoints = scalePointsUp(match.pts, scaleToOriginal);
            item.score = normalizeScore(match.similarity);
            item.angle = match.angle;
            item.scale = match.scale;

            item.center = match.transPt(matchCenter);
            item.center.x *= scaleToOriginal;
            item.center.y *= scaleToOriginal;
            item.Roiarea = rect = boundingRect(item.transformedFeaturePoints);;

            if (!item.transformedFeaturePoints.empty()) {
                cv::RotatedRect rr = cv::minAreaRect(item.transformedFeaturePoints);
                item.featureSize = rr.size;
                item.rotatedRect = rr;
            } else {
                item.featureSize = cv::Size2f(static_cast<float>(rect.width), static_cast<float>(rect.height));
                item.rotatedRect = cv::RotatedRect(
                    cv::Point2f(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f),
                    item.featureSize,
                    static_cast<float>(match.angle));
            }

            const double diag = std::hypot(static_cast<double>(item.featureSize.width),
                                           static_cast<double>(item.featureSize.height));
            const double distanceThreshold = std::max(10.0, diag * 0.3);
            if (isDuplicateMatch(item, results, distanceThreshold)) {
                continue;
            }

            results.push_back(item);
        }
        qInfo().noquote() << QStringLiteral("金字塔层 %1 匹配候选数: %2").arg(level).arg(static_cast<int>(matches.size()));
    }

    std::sort(results.begin(), results.end(), [](const MatchResult& lhs, const MatchResult& rhs) {
        return lhs.score > rhs.score;
    });
    if (static_cast<int>(results.size()) > maxCount) {
        results.resize(maxCount);
    }

    if (!results.empty()) {
        qInfo().noquote() << QStringLiteral("0~%1 金字塔轮询后输出结果数: %2")
                              .arg(scanUpperLevel)
                              .arg(static_cast<int>(results.size()));
    }

    return results;
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



