#pragma once
#include <QString>
#include <QJsonObject>
#include <opencv2/core.hpp>
#include <vector>
#include <memory>
namespace calibration { struct PerspectiveFitReport; }
namespace calibration { struct PlaneModel; }

// Internal data shared by the producer and consumer. No calibration-port types.
class MeasurementCalibration {
public:
    cv::Size imageSize;
    cv::Mat cameraMatrix, distCoeffs, newCameraMatrix, homography;
    double pixPerMm = 0.0;
    cv::Point2d offsetMm{0.0, 0.0};
    std::shared_ptr<calibration::PlaneModel> plane;
    QJsonObject regionMetadata; // optional v1 rendering/ROI metadata; point mapping unchanged

    bool hasCamera() const;
    bool ready() const;
    bool loadCamera(const QString& path);
    bool loadProfile(const QString& path);
    bool saveProfile(const QString& path) const;
    bool saveProfile(const QString& path, const calibration::PerspectiveFitReport* report) const;
    bool undistortImage(const cv::Mat& raw, cv::Mat& output) const;
    bool mapPoints(const std::vector<cv::Point2f>& raw,
                   std::vector<cv::Point2d>& millimeters) const;
};
