#pragma once
#include <QWidget>
#include <QPointF>
#include <QRegExpValidator>
#include "../Core/Warpective.h"
#include "../Scene/CameraWarpectiveScene.h"
#include "CameraWarptivePort.h"

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

    void ChangeShow();


private:

    bool isSaved = false;

    CameraCorrector *m_CameraCorrector; // 相机校正器实例
    Config m_config;

    WarpectiveScene* m_scene;

    std::vector<std::string> imagePaths;
    cv::Mat m_currentImage;
    cv::Mat m_rectifiedImage;
    bool isCurrentImageShow = true;
    bool isCalibrated = false; // 是否已标定成功

};