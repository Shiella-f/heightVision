#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <QString>

class MirrorCalib {
public:
    MirrorCalib();
    ~MirrorCalib();

    // 寻找标记点 (圆心或斑点)
    bool findMarkerPoints(const cv::Mat& image, cv::Size patternSize, std::vector<cv::Point2f>& centers);
    
    // --- 数据导出 ---
    bool saveComparisonToCSV(const QString& filepath, 
                             const std::vector<cv::Point2f>& detected, 
                             const std::vector<cv::Point2f>& preset);

    // 计算标定矩阵
    double calibrateHomography(const std::vector<cv::Point2f>& imagePoints, 
                               const std::vector<cv::Point2f>& physicalPoints, 
                               cv::Mat& outMatrix);

    // 保存标定结果到 XML/YAML
    bool saveCalibrationMatrix(const QString& filepath, const cv::Mat& matrix);

    bool displayimg = true;
};
