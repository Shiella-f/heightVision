// 实现文件负责处理双光斑测高逻辑。
#include "testHeight.h"

#include <filesystem>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>
#include <QMessageBox>

namespace Height::core {

namespace {
    const QStringList kImageFilters{
        "*.png",
        "*.jpg",
        "*.jpeg",
        "*.bmp",
        "*.tif",
        "*.tiff",
        "*.webp"
    };
}

bool HeightCore::loadFolder()
{
    // const QString folderPath = QFileDialog::getExistingDirectory(
    //     nullptr,
    //     QStringLiteral("选择图片文件夹"),
    //     QString(),
    //     QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    const QString folderPath = "E:/lipoDemo/HeiVersion/res/image";
    m_images.clear();

    if (folderPath.isEmpty()) {
        return false;
    }

    const QDir dir(folderPath); // 基于用户选择的路径构造目录对象，便于读取其中的文件
    QFileInfoList fileInfos = dir.entryInfoList(
        kImageFilters,                   // 仅匹配我们支持的图像后缀
        QDir::Files | QDir::Readable | QDir::NoSymLinks, // 只要可读的普通文件
        QDir::Name | QDir::IgnoreCase);   // 按名称排序，忽略大小写

    m_images.reserve(fileInfos.size());   // 预留空间，避免多次扩容
    for (const QFileInfo& fileInfo : fileInfos) {
        ImageInfo info;                   // 为每个文件创建一条记录
        info.path = fileInfo.absoluteFilePath().toLocal8Bit().toStdString(); // 保存绝对路径（转为 std::string）
        m_images.emplace_back(std::move(info)); // 将记录放入 m_images
    }

    sortImagesByName(); // 再次按路径排序，确保顺序稳定
    bool detected = detectTwoSpotsInImage(); // 载入后立即尝试识别光斑
    if (!detected) {
        QMessageBox::warning(nullptr, QStringLiteral("提示"), QStringLiteral("参考图像未在图像列表内"));
    }
    return !m_images.empty();
}



const std::vector<HeightCore::ImageInfo>& HeightCore::getImageInfos() const
{
    return m_images;
}

bool HeightCore::detectTwoSpotsInImage()
{
    if (m_images.empty()) {
        return false; // 没有输入图像，直接返回
    }

    m_lastDetectedCenters.clear(); // 清空上一轮检测到的圆心
    bool hasDetection = false;     // 记录是否至少检测到一个圆

    for (auto& imageInfo : m_images) {
        // 每次处理前先重置标记和坐标，避免沿用旧结果
        imageInfo.spot1Found = false;
        imageInfo.spot2Found = false;
        imageInfo.spot1Pos = cv::Point2f();
        imageInfo.spot2Pos = cv::Point2f();
        imageInfo.distancePx.reset();

        cv::Mat img = cv::imread(imageInfo.path, cv::IMREAD_COLOR);
        if (img.empty()) {
            continue; // 无法加载图像，跳过
        }

        // 预处理：直接在原始尺寸上获取灰度图并降噪
        cv::Mat gray;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY); // 转灰度
        cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);  // 高斯模糊抑制噪声

        // 自适应阈值 + 形态学闭运算，将光斑凸显出来
        cv::Mat thresh;
        threshold(gray, thresh, 100, 255, THRESH_BINARY);
        // namedWindow("Threshold", cv::WINDOW_NORMAL);
        // imshow("Threshold", thresh);
        // cv::waitKey(0);
        cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
        cv::Mat closed;
        cv::morphologyEx(thresh, closed, cv::MORPH_CLOSE, element); // 填补小孔，保留圆形区域
        // namedWindow("Closed", cv::WINDOW_NORMAL);
        // imshow("Closed", closed);
        //cv::waitKey(0);

        // 查找候选轮廓并基于面积/圆度进行筛选，同时准备亚像素求精
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(closed, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
        drawContours(img, contours, -1, cv::Scalar(255, 0, 0), 2); // 绘制所有轮廓以便调试观察
        cv::Mat tempimage = img.clone();
        cv::resize(tempimage, tempimage, cv::Size(800, 600));
        //imshow("Contours", tempimage);
        cv::waitKey(0);

        std::vector<std::pair<float, cv::Point2f>> candidates; // 暂存半径和圆心，便于后续筛选
        for (const auto& contour : contours) {
            const double area = cv::contourArea(contour);
                //qDebug() << "Contour area:" << area;
            if (area < 50.0 || area > 5000.0) {
                continue; // 忽略面积过小/过大的候选
            }

            const double perimeter = cv::arcLength(contour, true);
            if (perimeter <= 0.0) {
                continue;
            }

            const double circularity = 4.0 * CV_PI * area / (perimeter * perimeter);
            if (circularity < 0.5) {
                continue; // 过滤掉非圆形轮廓
            }

            // 使用最小外接圆拟合轮廓，得到近似圆心与半径
            cv::Point2f center;
            float radius = 0.0f;
            cv::minEnclosingCircle(contour, center, radius);

            // 通过角点细化(pxiel准确度 refine)获取亚像素级中心
            std::vector<cv::Point2f> contourPoints;
            contourPoints.reserve(contour.size());
            for (const cv::Point& pt : contour) {
                contourPoints.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
            }

            if (!contourPoints.empty()) {
                cv::cornerSubPix(gray,
                                 contourPoints,
                                 cv::Size(5, 5),
                                 cv::Size(-1, -1),
                                 cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT,
                                                  45,
                                                  0.01));

                // 使用细化后的点重新计算质心，作为亚像素中心
                double sx = 0.0;
                double sy = 0.0;
                for (const auto& pt : contourPoints) {
                    sx += pt.x;
                    sy += pt.y;
                }
                const double inv = 1.0 / static_cast<double>(contourPoints.size());
                center.x = static_cast<float>(sx * inv);
                center.y = static_cast<float>(sy * inv);
            }

            candidates.emplace_back(radius, center);
        }
        qDebug() << "candidates found:" << candidates.size();

        // 按半径从大到小排序，通常较大的圆对应有效光斑
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });

        // 只保留前两个圆心，符合“双光斑”业务需求
        std::vector<cv::Point2f> centersForImage;
        for (size_t i = 0; i < candidates.size() && i < 2; ++i) {
            centersForImage.push_back(candidates[i].second);
            const float radius = candidates[i].first;
            cv::Point2f center = candidates[i].second;

            // 在原图上画出圆心和圆轮廓，便于调试观察
            cv::circle(img, center, static_cast<int>(radius), cv::Scalar(0, 255, 0), 2);
            cv::circle(img, center, 3, cv::Scalar(0, 0, 255), cv::FILLED);
        }

        if (!centersForImage.empty()) {
            hasDetection = true;
            // 累积保存圆心结果，供外部查询或后续处理
            m_lastDetectedCenters.insert(m_lastDetectedCenters.end(),
                                         centersForImage.begin(), centersForImage.end());

            // 将结果写回 ImageInfo：第一、第二个光斑
            if (!centersForImage.empty()) {
                imageInfo.spot1Found = true;
                imageInfo.spot1Pos = centersForImage[0];
                if(centersForImage.size() == 1)
                {
                    imageInfo.spot2Found = false;
                    imageInfo.distancePx = 0;
                    imageInfo.spot2Pos = cv::Point2f();
                }
            }
            if (centersForImage.size() > 1) {
                imageInfo.spot2Found = true;
                imageInfo.spot2Pos = centersForImage[1];
                imageInfo.distancePx = computeDistancePx(centersForImage[0], centersForImage[1]);
            } else {
                imageInfo.spot2Found = false;
                imageInfo.distancePx.reset();
                imageInfo.spot2Pos = cv::Point2f();
            }

            // 展示处理效果图（包含拟合圆），快速确认检测质量
            pyrDown(img, img);
            cv::imshow("Detected Spots", img);
            auto key = cv::waitKey(0);
            if (key == 27) { // 按下 ESC 键退出
                break;
            }
        }
    }

    //排序
    rankImagesByDistancePx();
    return hasDetection;
}

double HeightCore::computeDistancePx(const cv::Point2f& a, const cv::Point2f& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(static_cast<double>(dx * dx + dy * dy));
}

void HeightCore::setStepMm(double stepMm)
{
    m_stepMm = stepMm;
}

double HeightCore::getStepMm() const
{
    return m_stepMm;
}

std::vector<double> HeightCore::getDistancePxList() const
{
    if (m_images.empty()) {
        qDebug() << "No images loaded.";
        return {};
    }
    std::vector<double> distancesPx;
    for (const auto& imageInfo : m_images) {
        if (imageInfo.distancePx.has_value()) {
            distancesPx.push_back(imageInfo.distancePx.value());
        }
        else {
            qDebug() <<  QString::fromStdString(imageInfo.path) << "does not have a valid distancePx.";
            return {};
        }
    }
    return distancesPx;
}

bool HeightCore::computeHeightValues()
{
    if (m_images.empty()) {
        qDebug() << "No images loaded.";
        return false;
    }
    if(m_referenceImgId <= -1)
        return false;
    if(m_images.size() <= m_referenceImgId)
        return false;
    if(!m_images[m_referenceImgId].distancePx.has_value())
        return false;

    for(int i = 0; i < m_referenceImgId; i++)
    {
        if(!m_images[i].distancePx.has_value())
            return false;
        m_images[i].heightMm = (m_images[m_referenceImgId].heightMm.value() - (m_referenceImgId - i) * m_stepMm);
    }

    for(int i = m_referenceImgId; i < m_images.size(); i++)
    {
        if(!m_images[i].distancePx.has_value())
            return false;
        double height = m_images[m_referenceImgId].heightMm.value() + (i - m_referenceImgId) * m_stepMm;
        m_images[i].heightMm = height;
    }

    return true;
}

std::vector<double> HeightCore::getHeightList() const
{
    if (m_images.empty()) {
        qDebug() << "No images loaded.";
        return {};
    }
    std::vector<double> heightsMm;
    for (const auto& imageInfo : m_images) {
        if (imageInfo.heightMm.has_value()) {
            heightsMm.push_back(imageInfo.heightMm.value());
        }
        else {
            qDebug() <<  QString::fromStdString(imageInfo.path) << "does not have a valid heightMm.";
            return {};
        }
    }
    return heightsMm;
}

bool HeightCore::importImageAsReference()
{
    const QString preferenceImagePath = QFileDialog::getOpenFileName(nullptr, ("打开图片"), "", ("Images (*.png *.xpm *.jpg *.bmp);;All Files (*)"));
    if (preferenceImagePath.isEmpty())
        return false;
    for(int i = 0; i < m_images.size(); ++i)
    {
        if(m_images[i].path == preferenceImagePath.toStdString())
        {
            m_referenceImgId = i;
            break;
        }
    }
    if (m_referenceImgId == -1)
    {
        QMessageBox::warning(nullptr, "警告", "参考图像未在图像列表内");
        return false;
    }
        
    // 加载参考图像
    m_preferenceImage = cv::imread(preferenceImagePath.toStdString());
    if (m_preferenceImage.empty())
        return false;

    return true;
}

bool HeightCore::setPreferenceHeight(double heightMm)
{
    if(m_referenceImgId <= -1)
        return false;
    if(m_images.size() <= m_referenceImgId)
        return false;
    if(!m_images[m_referenceImgId].distancePx.has_value())
        return false;

    m_images[m_referenceImgId].heightMm = heightMm;
    m_preferenceHeight = heightMm;
    return true;
}

double HeightCore::getPreferenceHeight() const
{
    return m_preferenceHeight;
}

bool HeightCore::setCalibrationLinear()
{
    vector<double> distancesPx = getDistancePxList();
    vector<double> heightsMm = getHeightList();

    if (distancesPx.size() != heightsMm.size() || distancesPx.empty()) {
        return false;
    }

    vector<Point2f> dataPoints;
    for (size_t i = 0; i < distancesPx.size(); ++i) {
        dataPoints.emplace_back(static_cast<float>(distancesPx[i]), static_cast<float>(heightsMm[i]));
    }
    cv::Vec4f lineParams;
    cv::fitLine(dataPoints, lineParams, cv::DIST_L2, 0, 0.01, 0.01);
    m_calibA = lineParams[1] / lineParams[0]; // 斜率
    m_calibB = lineParams[3] - m_calibA * lineParams[2]; // 截距

    return true;
}

void HeightCore::getCalibrationLinear(double& outA, double& outB) const
{
    outA = m_calibA;
    outB = m_calibB;
    qDebug() << "拟合直线:y = " << outA << "x + " << outB;
}

std::optional<double> HeightCore::measureHeightForImage(size_t index) const
{
    if (index >= m_images.size()) {
        return std::nullopt; // 索引越界
    }

    const ImageInfo& imageInfo = m_images[index];
    if (!imageInfo.spot1Found || !imageInfo.spot2Found || !imageInfo.distancePx.has_value()) {
        return std::nullopt; // 光斑未识别或距离不可用
    }

    // 优先使用线性标定
    if (m_hasLinearCalib) {
        double heightMm = m_calibA * imageInfo.distancePx.value() + m_calibB;
        return heightMm;
    }

    // 否则尝试使用像素->毫米比例转换
    if (m_pxToMm > 0.0) {
        double distanceMm = imageInfo.distancePx.value() * m_pxToMm;
        return distanceMm; // 作为高度返回
    }

    return std::nullopt; // 无法计算高度
}

void HeightCore::sortImagesByName()
{
    std::sort(m_images.begin(), m_images.end(), [](const ImageInfo& lhs, const ImageInfo& rhs) {
        return lhs.path < rhs.path;
    });
}

void HeightCore::rankImagesByDistancePx()
{
    if (m_images.empty()) {
        return;
    }
    for (const auto& imageInfo : m_images) 
    {
        if (!imageInfo.distancePx.has_value()) 
        {
            return;
        }
    }

    // 按 distancePx 从小到大排序
    std::sort(m_images.begin(), m_images.end(),
              [](const ImageInfo& lhs, const ImageInfo& rhs) 
              {
                  return lhs.distancePx.value() < rhs.distancePx.value();
              });
}

} // namespace Height::core
