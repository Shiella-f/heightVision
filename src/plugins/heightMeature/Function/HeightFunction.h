#pragma once
#include <QWidget>
#include <QtCore/qglobal.h>
#include <memory>
#include <optional>
#include <opencv2/opencv.hpp>
#include "../core/testHeight.h"
#include "HeightTestPort.h"
#include "../Scene/HeightScene.h"

#if defined(HEIGHTMEATURE_LIBRARY)
#  define HEIGHTMEATURE_EXPORT Q_DECL_EXPORT // 构建 DLL 时导出符号
#else
#  define HEIGHTMEATURE_EXPORT Q_DECL_IMPORT // 使用 DLL 时导入符号
#endif

class HeightScene;


class HEIGHTMEATURE_EXPORT HeightFunction : public QObject, public IHeightMeasure {
    Q_OBJECT

public:
    explicit HeightFunction();
    ~HeightFunction();

    Q_INVOKABLE bool isCalibrated() const override;
    
    // 新增公共接口
    bool LoadFolder(const QString& path) override;//加载文件夹图片
    double StartTest() override;//开始测高
    bool SelectImage(const QString &imagePath) override;//选择图片进行测高
    bool SetPreference(const QString& preferenceImagePath, double height) override;//设置基准图像及基准高度
    bool SelectROI() override;//启动ROI选择
    bool ConfirmROI() override;//确认ROI区域
    bool SaveCalibrationData(const QString& savePath) override;//保存标定数据
    bool LoadCalibrationData(const QString& loadPath) override;//加载标定数据
    double getMeasurementResult() const override;//获取测量结果
    bool setScene(HeightScene* scene);
    IHeightScene* getScene() const override;

private:
    void displayImage(const cv::Mat &image);
    void setHeightdisplay(const double& height);
    void GetROI();

private:
   
    HeightScene* m_zoomScene;
    std::unique_ptr<Height::core::HeightCore> m_heightCore;
    QString m_loadedFolder;
    std::optional<size_t> m_selectedImageIndex;

    double calibA;
    double calibB;
    cv::Mat m_showImage = cv::Mat();
    cv::Mat CameraImg = cv::Mat();
    cv::Mat currentCameraImg = cv::Mat();

    double m_TestHeight = -1.0;

    cv::Rect2f m_cvRoi;
    QTimer* m_CameraImgShowTimer = nullptr;
};