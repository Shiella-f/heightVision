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

bool HeightCore::loadFolder(const QString& folderPath)
{
    m_images.clear();
    setIsLinearCalib(false);

    if (folderPath.isEmpty()) {
        return false;
    }

    const QDir dir(folderPath);
    const QStringList kImageFilters{
        "*.png",
        "*.jpg",
        "*.jpeg",
        "*.bmp",
        "*.tif",
        "*.tiff",
        "*.webp"
    };
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
}

const std::vector<HeightCore::ImageInfo>& HeightCore::getImageInfos() const
{
    return m_images;
}

bool HeightCore::detectTwoSpotsInImage()
{
    if (m_images.empty()) {
        return false;
    }

    m_lastDetectedCenters.clear(); // 清空上一轮检测到的圆心
    bool hasDetection = false;     // 记录是否至少检测到一个圆

    for (auto& imageInfo : m_images) 
    {
        // 每次处理前先重置标记和坐标，避免沿用旧结果
        imageInfo.spot1Found = false;
        imageInfo.spot2Found = false;
        imageInfo.spot1Pos = cv::Point2f();
        imageInfo.spot2Pos = cv::Point2f();
        imageInfo.distancePx.reset();

        cv::Mat img = cv::imread(imageInfo.path, cv::IMREAD_COLOR);
        if (img.empty()) {
            qInfo() << "Failed to load image:" << QString::fromStdString(imageInfo.path);
            return false;
        }
        std::vector<std::pair<cv::Point2f, float>> detectedSpots;
        if (!processImage(img, detectedSpots)) {
            continue;
        }

        std::vector<std::pair<cv::Point2f, float>> spotsForImage;
        if (!selectBalancedPair(detectedSpots, kMaxAreaRatio, spotsForImage)) {
            continue;
        }

        if (!spotsForImage.empty()) 
        {
            hasDetection = true;
            // 累积保存圆心结果，供外部查询或后续处理
            for(const auto& spot : spotsForImage) {
                m_lastDetectedCenters.push_back(spot.first);
            }

            // 将结果写回 ImageInfo：第一、第二个光斑
            if (!spotsForImage.empty()) {
                imageInfo.spot1Found = true;
                // 记录第一光斑的圆心位置
                imageInfo.spot1Pos = spotsForImage[0].first;
                imageInfo.spot1Radius = spotsForImage[0].second;
            }
            if (spotsForImage.size() > 1) {
                imageInfo.spot2Found = true;
                // 记录第二光斑的圆心位置
                imageInfo.spot2Pos = spotsForImage[1].first;
                imageInfo.spot2Radius = spotsForImage[1].second;
                // 依据两个圆心计算像素距离
                imageInfo.distancePx = computeDistancePx(spotsForImage[0].first, spotsForImage[1].first);
            } else {
                imageInfo.spot2Found = false;
                imageInfo.distancePx.reset();
                imageInfo.spot2Pos = cv::Point2f();
                imageInfo.spot2Radius = 0.0f;
            }
            //展示处理效果图（包含拟合圆），快速确认检测质量
            if (processedIsDisplay) 
            {
                cv::Mat display = img.clone();
                for (const auto& spot : spotsForImage) 
                {
                    cv::circle(display, spot.first, 5, cv::Scalar(0, 255, 0), -1);
                }
                //pyrDown(display, display);
                cv::imshow("Detected Spots", display);
                auto key = cv::waitKey(0);
                if (key == 27) // 按下 ESC 键退出显示
                {
                    cv::destroyAllWindows();
                }
            } 
        }
    }

    // 保存参考图的路径，以便排序后恢复 ID (防止排序打乱索引)
    std::string refPath;
    if (m_referenceImgId >= 0 && m_referenceImgId < m_images.size()) {
        refPath = m_images[m_referenceImgId].path;
    }

    rankImagesByDistancePx();

    // 排序后重新查找参考图 ID
    if (!refPath.empty()) {
        for (int i = 0; i < m_images.size(); ++i) {
            if (m_images[i].path == refPath) {
                m_referenceImgId = i;
                break;
            }
        }
    }

    qDebug() << "Processed image count:" << m_images.size();
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
    qDebug()<<"dis";
    for(int i = 0; i < distancesPx.size() - 1; i++)
    {
        qDebug() <<distancesPx[i+1] - distancesPx[i];
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
    for (size_t i = 0; i < m_images.size(); ++i) {
        const auto& imageInfo = m_images[i];
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

bool HeightCore::importImageAsReference(const QString& imagePath)
{
    if (imagePath.isEmpty())
        return false;
    for(int i = 0; i < m_images.size(); ++i)
    {
        if(m_images[i].path == imagePath.toStdString())
        {
            m_referenceImgId = i;
            break;
        }
    }
    if (m_referenceImgId == -1)
    {
        return false;
    }
        
    // 加载参考图像
    m_preferenceImage = cv::imread(imagePath.toStdString());
    if (m_preferenceImage.empty())
        return false;
    m_showImage = m_preferenceImage.clone();
    return true;
}

bool HeightCore::setPreferenceHeight(double heightMm)
{
    if (!detectTwoSpotsInImage()) {
        return false;
    }

    if (m_images.empty()) {
        return false;
    }

    if (m_referenceImgId < 0 || static_cast<size_t>(m_referenceImgId) >= m_images.size()) {
        return false;
    }

    if (!m_images[m_referenceImgId].distancePx.has_value()) {
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
    std::vector<double> distancesPx = getDistancePxList();
    std::vector<double> heightsMm = getHeightList();

    if (distancesPx.size() != heightsMm.size() || distancesPx.empty()) {
        return false;
    }

    std::vector<cv::Point2f> dataPoints;
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

bool HeightCore::loadTestImageInfo(const QString& imagePath)
{
    if (imagePath.isEmpty())
        return false;

    m_testImageInfo.path = imagePath.toStdString();
    m_testImageInfo.image = cv::imread(m_testImageInfo.path, cv::IMREAD_COLOR);
     m_testImageInfo.spot1Found = false;
    m_testImageInfo.spot2Found = false;
    m_testImageInfo.spot1Pos = cv::Point2f();
    m_testImageInfo.spot2Pos = cv::Point2f();
    m_testImageInfo.distancePx.reset();
    m_showImage = m_testImageInfo.image.clone();
    return true;
}

bool HeightCore::loadTestImageInfo(const cv::Mat& image)
{
    if (image.empty())
        return false;

    m_testImageInfo.image = image.clone();
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
        return false;
    }

    m_lastDetectedCenters.clear(); // 清空上一轮检测到的圆心
    bool hasDetection = false;     // 记录是否至少检测到一个圆
    
    m_testImageInfo.spot1Found = false;
    m_testImageInfo.spot2Found = false;
    m_testImageInfo.spot1Pos = cv::Point2f();
    m_testImageInfo.spot2Pos = cv::Point2f();
    m_testImageInfo.distancePx.reset();
    std::vector<std::pair<cv::Point2f, float>> detectedSpots;
    if (!processImage(img, detectedSpots)) {
        return false;
    }

    std::vector<std::pair<cv::Point2f, float>> spotsForImage;
    if (!selectBalancedPair(detectedSpots, kMaxAreaRatio, spotsForImage)) {
        return false;
    }

    if (!spotsForImage.empty()) 
        {
            hasDetection = true;
            // 累积保存圆心结果，供外部查询或后续处理
            for(const auto& spot : spotsForImage) {
                m_lastDetectedCenters.push_back(spot.first);
            }

            // 将结果写回 ImageInfo：第一、第二个光斑
            if (!spotsForImage.empty()) 
            {
                m_testImageInfo.spot1Found = true;
                // 保存测试图像的第一圆心
                m_testImageInfo.spot1Pos = spotsForImage[0].first;
                m_testImageInfo.spot1Radius = spotsForImage[0].second;
                m_testImageInfo.distancePx = 0.0;
            }
            if (spotsForImage.size() > 1) 
            {
                m_testImageInfo.spot2Found = true;
                // 保存测试图像的第二圆心
                m_testImageInfo.spot2Pos = spotsForImage[1].first;
                m_testImageInfo.spot2Radius = spotsForImage[1].second;
                // 计算两个圆心的像素距离
                m_testImageInfo.distancePx = computeDistancePx(spotsForImage[0].first, spotsForImage[1].first);
            } else 
            {
                m_testImageInfo.spot2Found = false;
                m_testImageInfo.spot2Pos = cv::Point2f();
                m_testImageInfo.spot2Radius = 0.0f;
            }
            if (processedIsDisplay) 
            {
                cv::Mat display = m_showImage.clone();
                for (const auto& spot : spotsForImage) 
                {
                    cv::circle(display, spot.first, 5, cv::Scalar(0, 255, 0), -1);
                }
                cv::imshow("Detected Spots", display);
                cv::waitKey(0);}
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

    std::sort(m_images.begin(), m_images.end(),
              [](const ImageInfo& lhs, const ImageInfo& rhs) 
              {
                  return lhs.distancePx.value() < rhs.distancePx.value();
              });
}

bool HeightCore::processImage(const cv::Mat& input,
                              std::vector<std::pair<cv::Point2f, float>>& detectedSpots) const
{
    if (input.empty()) {
        return false;
    }

    // 1. 确定处理区域 (ROI)
    cv::Rect bounds(0, 0, input.cols, input.rows);
    cv::Rect roi;

    // 检查是否设置了有效的 ROI
    if (m_roi.width > 0 && m_roi.height > 0) {
        // 有 ROI：取交集，确保不越界
        // 显式转换为 cv::Rect (int) 以匹配 bounds
        roi = static_cast<cv::Rect>(m_roi) & bounds;
    } else {
        // 无 ROI：处理全图
        roi = bounds;
    }

    if (roi.width <= 0 || roi.height <= 0) {
        return false;
    }

    // 2. 计算坐标偏移量 (用于将 ROI 内坐标转换回全图坐标)
    cv::Point2f offset(static_cast<float>(roi.x), static_cast<float>(roi.y));

    // 3. 图像预处理
    cv::Mat gray;
    cv::cvtColor(input(roi), gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
    if(processedIsDisplay)
    {
        cv::Mat tempworking = gray.clone();
        if(tempworking.size().width < 800)
            cv::pyrUp(tempworking, tempworking);
        imshow("Blur", tempworking);
        cv::waitKey(0);
    }

    cv::Mat thresh;
    // 如果阈值设置不合理(<=0)，启用 Otsu 自动阈值
    // 激光光斑通常对比度极高，固定阈值通常更稳定，Otsu作备选
    double tVal = threshold;
    int tType = cv::THRESH_BINARY;

    if (tVal <= 0) {
        tVal = 0;
        tType |= cv::THRESH_OTSU;
    }
    cv::threshold(gray, thresh, tVal, 255, tType);
    if(processedIsDisplay)
    {
        cv::Mat tempworking = thresh.clone();
        if(tempworking.size().width < 800)
            cv::pyrUp(tempworking, tempworking);
        imshow("thresh", tempworking);
        cv::waitKey(0);
    }

    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::Mat closed;
    cv::morphologyEx(thresh, closed, cv::MORPH_CLOSE, element, cv::Point(-1, -1), 2);
    if(processedIsDisplay)
    {
        cv::Mat tempworking = closed.clone();
        if(tempworking.size().width < 800)
            cv::pyrUp(tempworking, tempworking);
        imshow("closed", tempworking);
        cv::waitKey(0);
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(closed, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE); // 改为只检测外轮廓，减少嵌套干扰
    
    detectedSpots.clear();
    detectedSpots.reserve(contours.size());

    for (const auto& contour : contours) {
        // 1. 基础几何筛选
        double area = cv::contourArea(contour);
        if (area < minArea || area > maxArea) {
            qDebug() << "Filtered by area:" << area;
            continue;
        }

        double perimeter = cv::arcLength(contour, true);
        if (perimeter <= 0.0) {
            continue;
        }

        // 圆度,完美圆为 1.0
        double circularity = 4.0 * CV_PI * area / (perimeter * perimeter);
        
        // 提高圆度阈值，激光光斑通常比较圆
        if (circularity < 0.6) { 
            qDebug() << "Filtered by circularity:" << circularity;
            continue;
        }

        // 增加惯性比检查，排除过于细长的干扰（如划痕反光）
        if (contour.size() >= 5) {
            cv::RotatedRect rotRect = cv::fitEllipse(contour);
            float w = rotRect.size.width;
            float h = rotRect.size.height;
            if (w > 0 && h > 0) {
                float inertiaRatio = std::min(w, h) / std::max(w, h);
                if (inertiaRatio < 0.2) { // 长宽比限制在 1:5 以内
                    continue;
                }
            }
        }

        cv::Rect boundingRect = cv::boundingRect(contour);
        // 确保 ROI 在图像范围内
        boundingRect = boundingRect & cv::Rect(0, 0, gray.cols, gray.rows);
        if (boundingRect.area() <= 0) continue;

        // 提取 ROI 进行局部处理，提高效率
        cv::Mat roiImg = gray(boundingRect);
        cv::Mat roiMask = cv::Mat::zeros(roiImg.size(), CV_8UC1);
        
        // 在 ROI mask 上绘制填充的轮廓
        std::vector<cv::Point> roiContour;
        roiContour.reserve(contour.size());
        for(const auto& pt : contour) {
            roiContour.emplace_back(pt.x - boundingRect.x, pt.y - boundingRect.y);
        }
        std::vector<std::vector<cv::Point>> roiContours = {roiContour};
        cv::drawContours(roiMask, roiContours, 0, cv::Scalar(255), cv::FILLED);

        // 计算 ROI 内的灰度矩
        // 使用 mask 确保只计算光斑内部的像素，排除背景噪声
        cv::Mat maskedRoi;
        roiImg.copyTo(maskedRoi, roiMask);
        
    
        // 将像素值平方，可以抑制低亮度的噪声，突出高亮度的中心，提高亚像素精度
        cv::Mat squaredRoi;
        maskedRoi.convertTo(squaredRoi, CV_32F);
        cv::pow(squaredRoi, 1, squaredRoi);//使用平方加权质心法 (pow 2 会过度放大亮度差异，改为 pow 1 保持线性)

        cv::Moments mu = cv::moments(squaredRoi, false); 

        if (mu.m00 <= 1e-5) continue; // 避免除零

        // 计算局部重心坐标
        float cx = static_cast<float>(mu.m10 / mu.m00);
        float cy = static_cast<float>(mu.m01 / mu.m00);

        // 转换回全局坐标
        cv::Point2f finalCenter(cx + boundingRect.x + offset.x, cy + boundingRect.y + offset.y);

        // 估算半径 (使用等效面积圆半径)
        float radius = std::sqrt(static_cast<float>(area) / CV_PI);

        detectedSpots.emplace_back(finalCenter, radius);
    }

    if (detectedSpots.empty()) return false;

    std::sort(detectedSpots.begin(), detectedSpots.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });
    return true;
}

bool HeightCore::selectBalancedPair(const std::vector<std::pair<cv::Point2f, float>>& spots,
                            double maxAreaRatio,
                            std::vector<std::pair<cv::Point2f, float>>& outSpots) const
    {
        outSpots.clear();
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
                if (ratio <= ratioLimit)
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
            selected = {0};
        }

        if (selected.empty())
        {
            return false;
        }

        outSpots.reserve(selected.size());
        for (size_t idx : selected) 
        {
            outSpots.push_back(spots[idx]);
        }
        return !outSpots.empty();
    }

bool HeightCore::saveCalibrationData(const QString& filePath) const
{
    if (filePath.isEmpty())
        return false;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    if(!m_hasLinearCalib)
    {
        return false;
    }
    out << m_calibA << " " << m_calibB << "\n";
    file.close();

    if (out.status() != QTextStream::Ok) {
        return false;
    }

    return true;
}

bool HeightCore::loadCalibrationData(const QString& filePath)
{
    if (filePath.isEmpty())
        return false;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    double a = 0.0;
    double b = 0.0;
    in >> a >> b;
    file.close();

    if (in.status() != QTextStream::Ok) {
        return false;
    }

    m_calibA = a;
    m_calibB = b;
    setIsLinearCalib(true);
    
    return true;
}


} // namespace Height::core
