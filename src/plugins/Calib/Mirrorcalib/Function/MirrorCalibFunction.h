#pragma once
#include "Mirrorcalib/Core/MirrorCalib.h"
#include "../Scene/MirrorScene.h"
#include "MirrorCalibPort.h"
#include "../../MeasurementCalibration.h"

class MirrorCalibFunction : public QObject, public CalibWarptivePort {
    Q_OBJECT
public:
    explicit MirrorCalibFunction(QWidget* parent = nullptr);
    bool LoadCalibrationData(const QString& path) override;
    bool Calibrate(const cv::Mat& image, int gridSize) override;
    bool Calibrate(const QString& imagePath, int gridSize) override;
    QVector<QPointF> GetPoints() const override;
    QVector<QPointF> GetPhysicalPoints() const override;
    bool SaveResult(const QString& savePath) const override;
    IMirrorScene* getScene() const override;

private:
    void clearResult();
    MirrorCalib m_core;
    MirrorScene* m_scene = nullptr;
    std::vector<cv::Point2f> m_points;
    std::vector<cv::Point2d> m_physicalPoints;
    MeasurementCalibration m_calibration;
};
