#pragma once
#include <QWidget>
#include <QPointF>

#include "../Core/Warpective.h"
#include "../Scene/CameraWarpectiveScene.h"
#include "CameraWarptivePort.h"
#include "../../MeasurementCalibration.h"
#include "../../PerspectiveRemap.h"

// 相机变换界面类
class CameraWarpectiveFunction : public QObject, public CameraWarptivePort
{
      Q_OBJECT
public:
    explicit CameraWarpectiveFunction(QWidget *parent = nullptr);
    ~CameraWarpectiveFunction();
    // 通过接口暴露的标定函数（通过虚函数调用）
    bool CalibWarptive(const Config& config, const cv::Mat& srcImage) override;
    bool SaveResult(const QString savePath) override;
    bool UseWarptiveImage(const cv::Mat& srcImage, cv::Mat& dstImage) const override; // 获取校正后的图像
    ICameraWarptiveSeene* getScene() const override; // 返回场景指针
    bool LoadCameraCalibrationData(const QString& path) override;
    bool SaveMeasurementCalibration(const QString& path) const override;
    bool MapInputToBoard(const QPointF& input,QPointF& boardMm) const override;
    bool SetOutputRegion(const QPointF& centerMm,double widthMm,double heightMm,double pixelsPerMm) override;
    QJsonObject GetReport() const override;

    void ChangeShow();
signals:
    void calibrationLog(const QString& message) const;


private:

    bool isSaved = false;

    CameraCorrector *m_CameraCorrector; // 相机校正器实例
    MeasurementCalibration m_measurement;
    calibration::PerspectiveRemapper m_remapper;
    Config m_config;
    cv::Mat m_boardHomography;
    MeasurementCalibration m_renderMeasurement;
    cv::Size m_outputSize;
    QJsonObject m_report;
    std::vector<cv::Point2f> m_supportHull;
    bool m_regionValid=false;
    int m_detectionCalls=0;

    WarpectiveScene* m_scene;

    std::vector<std::string> imagePaths;
    cv::Mat m_currentImage;
    cv::Mat m_rectifiedImage;
    bool isCurrentImageShow = true;
    bool isCalibrated = false; // 是否已标定成功

};
