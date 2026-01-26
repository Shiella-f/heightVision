#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <QString>

class MirrorCalib {
public:
    MirrorCalib();
    ~MirrorCalib();

    // 寻找标记点 (圆心或斑点)
    // image: 输入图片
    // patternSize: 角点数量 (例如 cv::Size(3, 3) 表示角点 3列 3行)
    // centers: 输出标记点坐标
    bool findMarkerPoints(const cv::Mat& image, cv::Size patternSize, std::vector<cv::Point2f>& centers);
    
    // --- 数据导出 ---
    // filepath: 输出文件路径
    // detected: 检测到的点坐标
    // preset: 预设点坐标
    bool saveComparisonToCSV(const QString& filepath, 
                             const std::vector<cv::Point2f>& detected, 
                             const std::vector<cv::Point2f>& preset);

    // 计算标定矩阵
    // imagePoints: 检测到的图像点
    // physicalPoints: 物理点
    // outMatrix: 输出的单应矩阵
    double calibrateHomography(const std::vector<cv::Point2f>& imagePoints, 
                               const std::vector<cv::Point2f>& physicalPoints, 
                               cv::Mat& outMatrix);

    // 保存标定结果到 XML/YAML
    // filepath: 输出文件路径
    // matrix: 标定矩阵
    bool saveCalibrationMatrix(const QString& filepath, const cv::Mat& matrix);

    bool displayimg = true;
};
