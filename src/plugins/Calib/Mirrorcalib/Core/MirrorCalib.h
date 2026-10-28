#pragma once
#include <opencv2/opencv.hpp>
#include <QString>
#include <vector>

class MirrorCalib {
public:
    static bool isValidGridSize(int gridSize);
    bool findMarkerPoints(const cv::Mat& image, int gridSize,
                          std::vector<cv::Point2f>& centers) const;
    bool savePointsToCSV(const QString& filepath,
                         const std::vector<cv::Point2f>& points,
                         const std::vector<cv::Point2d>& physicalPoints) const;
};
