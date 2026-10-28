#pragma once
#include <opencv2/core.hpp>
#include <QJsonObject>
#include <QStringList>
#include <vector>

namespace calibration {
// A calibration of one fixed physical plane. This is not a pinhole camera model.
struct PlaneModel {
    cv::Size imageSize;
    double normalizationScale = 0;
    cv::Point2d distortionCenter;
    cv::Vec2d radial{0, 0};
    cv::Mat worldHomography; // undistorted normalized coordinates -> reference board mm
    std::vector<cv::Point2f> supportHull, observations;
    QJsonObject board, report;
    QStringList trainingHashes;
    QString referenceImage;
    bool regionValid = false;
    cv::Point2d regionCenter;
    cv::Size2d regionSize{310, 310};
    double requestedPixelsPerMm = 10;
    cv::Size outputSize;

    bool valid() const;
    double radiusLimit() const;
    bool radialValid(double radius) const;
    bool rawToWorld(cv::Point2d raw, cv::Point2d& world, bool checkSupport = false) const;
    bool worldToRaw(cv::Point2d world, cv::Point2d& raw, bool checkImage = true) const;
    bool mapPoints(const std::vector<cv::Point2f>& raw, std::vector<cv::Point2d>& mm) const;
    bool mapPoints(const std::vector<cv::Point2d>& raw, std::vector<cv::Point2d>& mm) const;
    bool setRegion(cv::Point2d center, cv::Size2d size, double density, QString& error);
    std::vector<cv::Point2d> regionBoundary() const;
    bool buildMaps(cv::Mat& x, cv::Mat& y) const;
    QString fingerprint(bool includeRegion = true) const;
    bool save(const QString& path, bool measurement) const;
    static bool load(const QString& path, PlaneModel& model, bool requireRegion, QString& error);
};
}
