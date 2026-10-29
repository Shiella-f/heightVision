#pragma once
#include <QString>
#include <QJsonObject>
#include <opencv2/core.hpp>
#include <vector>
#include <memory>
#include "PostPerspectiveRoi.h"
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
    calibration::PostPerspectiveRoi postRoi;
    cv::Size2d outputMm;
    bool halfPixelOutput=false;
    bool loadPerspective(const QString& path);
    cv::Size outputPixels()const;
    cv::Mat pixelToMm()const;
    cv::Mat renderHomography()const;
    QString roiBinding()const;
    bool validatePostRoi(QString& error)const;

    bool hasCamera() const;
    bool ready() const;
    bool loadCamera(const QString& path);
    bool loadProfile(const QString& path);
    // Mirror detection uses the base canvas and intentionally ignores post_perspective_roi.
    bool loadBaseProfile(const QString& path);
    bool saveProfile(const QString& path) const;
    bool saveProfile(const QString& path, const calibration::PerspectiveFitReport* report) const;
    bool undistortImage(const cv::Mat& raw, cv::Mat& output) const;
    bool mapPoints(const std::vector<cv::Point2f>& raw,
                   std::vector<cv::Point2d>& millimeters) const;
private:
    bool loadProfileImpl(const QString& path, bool applyPostRoi);
};
