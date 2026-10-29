#pragma once
#include <QJsonObject>
#include "PostPerspectiveRoiPort.h"
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QVector>
#include <opencv2/core.hpp>
#include <functional>

#if defined(CalibPLUGIN_LIBRARY)
#define PLANECALIB_EXPORT Q_DECL_EXPORT
#else
#define PLANECALIB_EXPORT Q_DECL_IMPORT
#endif

// A fixed camera observing one fixed-height plane; no physical camera intrinsics are claimed.
class PlaneCalibrationPort {
public:
    struct Region {
        QPointF centerMm{0,0};
        double widthMm=310, heightMm=310, pixelsPerMm=10;
    };
    virtual ~PlaneCalibrationPort() = default;
    virtual void SetProgressCallback(std::function<void(const QString&)> callback) = 0;
    virtual bool PreviewBoard(const QJsonObject& board, cv::Mat& image) = 0;
    virtual bool Calibrate(const QVector<QString>& images, const QJsonObject& board,
                           const QString& referenceImage) = 0;
    virtual bool LoadCalibrationData(const QString& path) = 0;
    virtual bool SaveCalibrationData(const QString& path) const = 0;
    virtual bool SetRegion(const Region& region) = 0;
    virtual bool MapPoints(const QVector<QPointF>& raw, QVector<QPointF>& millimeters) const = 0;
    virtual bool MapRawToReference(const QPointF& raw, QPointF& boardMm) const = 0;
    virtual QVector<QPointF> GetRegionBoundary() const = 0; // original pixels
    virtual bool RectifyImage(const cv::Mat& raw, cv::Mat& result) const = 0;
    // Optional independent references JSON: source, independent, points/distances; see SDK docs.
    virtual bool Validate(const QVector<QString>& heldOutImages, const QJsonObject& physicalReferences = {}) = 0;
    virtual QJsonObject GetReport() const = 0;
    virtual bool SaveMeasurementCalibration(const QString& path) const = 0;
    // Image-supported board inspection only; leaves model, ROI and acceptance untouched.
    virtual bool InspectBoard(const cv::Mat& image,const QJsonObject& board,QJsonObject& report) const = 0;
    virtual bool SetPostPerspectiveRoi(const PostPerspectiveRoiConfig& config) = 0;
    virtual void ClearPostPerspectiveRoi() = 0;
    virtual bool RenderBaseImage(const cv::Mat& input,cv::Mat& output) const = 0;
protected:
    PlaneCalibrationPort() = default;
};
PLANECALIB_EXPORT PlaneCalibrationPort* newPlaneCalibrationPort();
