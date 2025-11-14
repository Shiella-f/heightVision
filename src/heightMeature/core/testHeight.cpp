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

} // namespace

bool HeightCore::loadFolder()
{
    // const QString folderPath = QFileDialog::getExistingDirectory(
    //     nullptr,
    //     QStringLiteral("选择图片文件夹"),
    //     QString(),
    //     QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    const QString folderPath = "E:/lipoDemo/HeiVersion/res/image";
    m_images.clear();
    setIsLinearCalib(false);

    if (folderPath.isEmpty()) {
        return false;
    }

    const QDir dir(folderPath);
    QFileInfoList fileInfos = dir.entryInfoList(
        kImageFilters,
        QDir::Files | QDir::Readable | QDir::NoSymLinks,
        QDir::Name | QDir::IgnoreCase);

    m_images.reserve(fileInfos.size());
    for (const QFileInfo& fileInfo : fileInfos) {
        ImageInfo info;
        info.path = fileInfo.absoluteFilePath().toLocal8Bit().toStdString();
        m_images.emplace_back(std::move(info));
    }

    sortImagesByName();
    m_showImage = m_images.empty() ? cv::Mat() : cv::imread(m_images[0].path, cv::IMREAD_COLOR);
    return !m_images.empty();

    // bool detected = detectTwoSpotsInImage();
    // if (!detected) {
    //     QMessageBox::warning(nullptr, QStringLiteral("提示"), QStringLiteral("图像处理错误"));
    // }
    // return !m_images.empty();
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
        std::vector<std::pair<cv::Point2f, float>> detectedSpots;
        cv::Mat debugView;
        if (!processImage(img, detectedSpots, &debugView)) {
            continue;
        }

        std::vector<cv::Point2f> centersForImage;
        const double kMaxAreaRatio = 10; // 控制两个圆面积的最大允许比值
        if (!selectBalancedPair(detectedSpots, kMaxAreaRatio, centersForImage,
                                 debugView.empty() ? nullptr : &debugView)) {
            continue;
        }

        if (!centersForImage.empty()) 
        {
            hasDetection = true;
            // 累积保存圆心结果，供外部查询或后续处理
            m_lastDetectedCenters.insert(m_lastDetectedCenters.end(),
                                         centersForImage.begin(), centersForImage.end());

            // 将结果写回 ImageInfo：第一、第二个光斑
            if (!centersForImage.empty()) {
                imageInfo.spot1Found = true;
                // 记录第一光斑的圆心位置
                imageInfo.spot1Pos = centersForImage[0];
            }
            if (centersForImage.size() > 1) {
                imageInfo.spot2Found = true;
                // 记录第二光斑的圆心位置
                imageInfo.spot2Pos = centersForImage[1];
                // 依据两个圆心计算像素距离
                imageInfo.distancePx = computeDistancePx(centersForImage[0], centersForImage[1]);
            } else {
                imageInfo.spot2Found = false;
                imageInfo.distancePx.reset();
                imageInfo.spot2Pos = cv::Point2f();
            }

            // 展示处理效果图（包含拟合圆），快速确认检测质量
            if (!debugView.empty() && resultIsDisplay) {
                cv::Mat display = debugView.clone();
                pyrDown(display, display);
                cv::imshow("Detected Spots", display);
                auto key = cv::waitKey(0);
                if (key == 27) { // 按下 ESC 键退出
                    break;
                }
            } else {
                continue; // 未要求显示调试图时，继续处理下一张图片
            }
        }
    }

    //排序
    rankImagesByDistancePx();
    qDebug() << "Processed image count:" << m_images.size();
    for (const auto& info : m_images) {
        qDebug() 
                 << "Spot1 Pos:(" << info.spot1Pos.x << ", " << info.spot1Pos.y << ")"
                 << "Spot2 Pos:(" << info.spot2Pos.x << ", " << info.spot2Pos.y << ")"
                 << "Distance Px:" << (info.distancePx.has_value() ? QString::number(info.distancePx.value()) : "N/A")
                 << "Height Mm:" << (info.heightMm.has_value() ? QString::number(info.heightMm.value()) : "N/A")
                 << "-----------------------------------";
    }
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
    if(!m_images[m_referenceImgId].heightMm.has_value())
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
    m_showImage = m_preferenceImage.clone();
    return true;
}

bool HeightCore::setPreferenceHeight(double heightMm)
{
    if (!detectTwoSpotsInImage()) {
        QMessageBox::warning(nullptr, QStringLiteral("提示"), QStringLiteral("图像处理错误"));
        return false;
    }

    if (m_images.empty()) {
        return false;
    }

    if (m_referenceImgId < 0 || static_cast<size_t>(m_referenceImgId) >= m_images.size()) {
        QMessageBox::warning(nullptr, QStringLiteral("提示"), QStringLiteral("参考图片索引无效"));
        return false;
    }

    if (!m_images[m_referenceImgId].distancePx.has_value()) {
        QMessageBox::warning(nullptr, QStringLiteral("提示"), QStringLiteral("参考图片未检测到有效光斑"));
        return false;
    }

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

bool HeightCore::loadTestImageInfo()
{
    // const QString m_testImagePath = QFileDialog::getOpenFileName(nullptr, ("打开图片"), "", ("Images (*.png *.xpm *.jpg *.bmp);;All Files (*)"));
    // if (m_testImagePath.isEmpty())
    //     return false;

    // m_testImageInfo.path = m_testImagePath.toStdString();
    m_testImageInfo.path=("E:\\lipoDemo\\HeiVersion\\res\\test\\img\\9.jpg");
    m_testImageInfo.image = cv::imread(m_testImageInfo.path, cv::IMREAD_COLOR);
     m_testImageInfo.spot1Found = false;
    m_testImageInfo.spot2Found = false;
    m_testImageInfo.spot1Pos = cv::Point2f();
    m_testImageInfo.spot2Pos = cv::Point2f();
    m_testImageInfo.distancePx.reset();
    m_showImage = m_testImageInfo.image.clone();
    return true;
}

bool HeightCore::computeTestImageInfo()
{
    cv::Mat img = m_testImageInfo.image;
    if (img.empty()) {
        return false; // 无法加载图像，跳过
    }

    m_lastDetectedCenters.clear(); // 清空上一轮检测到的圆心
    bool hasDetection = false;     // 记录是否至少检测到一个圆
    
    m_testImageInfo.spot1Found = false;
    m_testImageInfo.spot2Found = false;
    m_testImageInfo.spot1Pos = cv::Point2f();
    m_testImageInfo.spot2Pos = cv::Point2f();
    m_testImageInfo.distancePx.reset();
    std::vector<std::pair<cv::Point2f, float>> detectedSpots;
    cv::Mat debugView;
    if (!processImage(img, detectedSpots, &debugView)) {
        return false;
    }

    std::vector<cv::Point2f> centersForImage;
    const double kMaxAreaRatio = 1.5; // 控制两个圆面积的最大允许比值
    if (!selectBalancedPair(detectedSpots, kMaxAreaRatio, centersForImage,
                             debugView.empty() ? nullptr : &debugView)) {
        return false;
    }
    //qDebug() << "centersForImage size:" << centersForImage.size()<< centersForImage[0].x;

    if (!centersForImage.empty()) 
        {
            hasDetection = true;
            // 累积保存圆心结果，供外部查询或后续处理
            m_lastDetectedCenters.insert(m_lastDetectedCenters.end(),
                                         centersForImage.begin(), centersForImage.end());

            // 将结果写回 ImageInfo：第一、第二个光斑
            if (!centersForImage.empty()) 
            {
                m_testImageInfo.spot1Found = true;
                // 保存测试图像的第一圆心
                m_testImageInfo.spot1Pos = centersForImage[0];
                m_testImageInfo.distancePx = 0.0;
            }
            if (centersForImage.size() > 1) 
            {
                m_testImageInfo.spot2Found = true;
                // 保存测试图像的第二圆心
                m_testImageInfo.spot2Pos = centersForImage[1];
                // 计算两个圆心的像素距离
                m_testImageInfo.distancePx = computeDistancePx(centersForImage[0], centersForImage[1]);
            } else 
            {
                m_testImageInfo.spot2Found = false;
                m_testImageInfo.spot2Pos = cv::Point2f();
            }
            qDebug() << "Spot1:" << m_testImageInfo.spot1Pos.x << "," << m_testImageInfo.spot1Pos.y;
            qDebug() << "Spot2:" << m_testImageInfo.spot2Pos.x << "," << m_testImageInfo.spot2Pos.y;
            if (m_testImageInfo.distancePx.has_value()) {
                qDebug() << "DistancePx:" << m_testImageInfo.distancePx.value();
            }
            // 展示处理效果图（包含拟合圆），快速确认检测质量
            if (!debugView.empty()) 
            {
                cv::Mat display = debugView.clone();
                pyrDown(display, display);
                cv::imshow("Detected Spots", display);
            } else 
            {
                return false;
            }
            return true;
        }
    return false;
}

std::optional<double> HeightCore::measureHeightForImage() const
{
    if(m_testImageInfo.spot1Found && m_testImageInfo.spot2Found)
    {
        double distancePx = computeDistancePx(m_testImageInfo.spot1Pos, m_testImageInfo.spot2Pos);
        if (!m_hasLinearCalib) {
            QMessageBox::warning(nullptr, "警告", "未设置线性标定参数");
            return std::nullopt;

        }
        // 使用线性标定计算高度
            double heightMm = m_calibA * distancePx + m_calibB;
            return heightMm;
    }
    return std::nullopt;
}

void HeightCore::setROI(const cv::Rect2f& roi)
{
    m_roi = roi;
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

bool HeightCore::processImage(const cv::Mat& input,
                              std::vector<std::pair<cv::Point2f, float>>& detectedSpots,
                              cv::Mat* debugOutput) const
{
    if (input.empty()) {
        return false;
    }

    cv::Rect bounds(0, 0, input.cols, input.rows);
    cv::Rect roi = (m_roi.width > 0 && m_roi.height > 0) ? (m_roi & bounds) : bounds;
    if (roi.width <= 0 || roi.height <= 0) {
        return false;
    }

    cv::Mat working = input(roi).clone();
    //imshow("working", working);
    cv::Point2f offset(static_cast<float>(roi.x), static_cast<float>(roi.y));

    cv::Mat gray;
    cv::cvtColor(working, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
    if(processedIsDisplay)
    {
    cv::Mat tempblue = gray.clone();
    imshow("blur", tempblue);
    cv::waitKey(0);
    }


    cv::Mat thresh;
    cv::threshold(gray, thresh, threshold, 255, cv::THRESH_BINARY);
    if(processedIsDisplay)
    {
    cv::Mat tempthresh = thresh.clone();
    imshow("thresh", tempthresh);
    cv::waitKey(0);
    }

    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
    cv::Mat closed;
    cv::morphologyEx(thresh, closed, cv::MORPH_CLOSE, element);
    if(processedIsDisplay)
    {
    cv::Mat tempimg = closed.clone();
    imshow("tempimg", tempimg);
    cv::waitKey(0);
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(closed, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
    drawContours(working, contours, -1, cv::Scalar(0, 255, 0), 2);
    // imshow("contours", working);
    // cv::waitKey(0);

    detectedSpots.clear();
    detectedSpots.reserve(contours.size());
    std::vector<std::vector<cv::Point>> deal_contours;
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area < 10.0 || area > 10000.0) {
            continue;
        }

        double perimeter = cv::arcLength(contour, true);
        if (perimeter <= 0.0) {
            continue;
        }

        double circularity = 4.0 * CV_PI * area / (perimeter * perimeter);
        qDebug() << "Rejected contour with low circularity:" << circularity;

        if (circularity < 0.5) {
            continue;
        }

        cv::Point2f rawCenter;
        float radius = 0.0f;
        // 通过最小外接圆得到初始圆心与半径
        cv::minEnclosingCircle(contour, rawCenter, radius);

        std::vector<cv::Point2f> refined;
        refined.reserve(contour.size());
        for (const cv::Point& pt : contour) {
            // 将轮廓点转换为浮点坐标以便亚像素求精
            refined.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
        }

        if (!refined.empty()) {
            // 使用角点细化算法提高圆心精度
            cv::cornerSubPix(gray,refined, cv::Size(5, 5), cv::Size(-1, -1),
                             cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 45, 0.01));
        }

        cv::Point2f refinedCenter = rawCenter;
        if (!refined.empty()) {
            double sx = 0.0;
            double sy = 0.0;
            for (const auto& pt : refined) {
                // 累加亚像素坐标用于求平均中心
                sx += pt.x;
                sy += pt.y;
            }
            const double inv = 1.0 / static_cast<double>(refined.size());
            // 取平均值作为最终圆心坐标
            refinedCenter.x = static_cast<float>(sx * inv);
            refinedCenter.y = static_cast<float>(sy * inv);
        }

        // 将圆心从 ROI 坐标系映射回整幅图像
        refinedCenter += offset;
        // 保存圆心与半径供外部筛选
        detectedSpots.emplace_back(refinedCenter, radius);
    }

    if (detectedSpots.empty()) {
        if (debugOutput) {
            *debugOutput = input.clone();
        }
        return false;
    }

    std::sort(detectedSpots.begin(), detectedSpots.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

    if (debugOutput) {
        *debugOutput = input.clone();
        if (roi != bounds) {
            cv::rectangle(*debugOutput, roi, cv::Scalar(0, 128, 255), 1);
        }
    }

    return true;
}

bool HeightCore::selectBalancedPair(const std::vector<std::pair<cv::Point2f, float>>& spots,
                            double maxAreaRatio,
                            std::vector<cv::Point2f>& outCenters,
                            cv::Mat* debugView) const
    {
        outCenters.clear();
        const double ratioLimit = std::max(1.0, maxAreaRatio);
        std::vector<size_t> selected;
        bool found = false;
        for (size_t i = 0; i + 1 < spots.size(); ++i) 
        {
            for (size_t j = i + 1; j < spots.size(); ++j) 
            {
                const float r1 = spots[i].second;
                const float r2 = spots[j].second;
                if (r1 <= 0.0f || r2 <= 0.0f) 
                {
                    continue;
                }

                const double area1 = CV_PI * static_cast<double>(r1) * static_cast<double>(r1);
                const double area2 = CV_PI * static_cast<double>(r2) * static_cast<double>(r2);
                const double ratio = area1 > area2 ? (area1 / area2) : (area2 / area1);
                const double dx = spots[i].first.x - spots[j].first.x;
                const double dy = spots[i].first.y - spots[j].first.y;
                const double distance = std::sqrt(dx * dx + dy * dy);
                if (ratio <= ratioLimit && distance < 230.0) 
                {
                    selected = {i, j};
                    found = true;
                    break;
                }
            }
            if (found) 
            {
                break;
            }
        }

        if (!found)
        {
            // 未找到符合比例的圆对时，回退为选择首个候选圆（通常为最大半径）
            selected = {0};
        }

        if (selected.empty())
        {
            return false;
        }

        outCenters.reserve(selected.size());
        for (size_t idx : selected) 
        {
            outCenters.push_back(spots[idx].first);
        }

        if (debugView && !debugView->empty()) 
        {
            for (size_t idx : selected) 
            {
                const cv::Point2f& center = spots[idx].first;
                const float radius = spots[idx].second;
                cv::circle(*debugView, center, static_cast<int>(radius), cv::Scalar(0, 255, 0), 2);
                cv::circle(*debugView, center, 3, cv::Scalar(0, 0, 255), cv::FILLED);
            }
        }

        return !outCenters.empty();
    }

} // namespace Height::core
