#pragma once
#include <QString>
#include <QRectF>
#include <QImage>
#include <QtCore/qglobal.h>

#if defined(HEIGHTMEATURE_LIBRARY)
#  define HEIGHTMEATURE_EXPORT Q_DECL_EXPORT
#else
#  define HEIGHTMEATURE_EXPORT Q_DECL_IMPORT
#endif

class QGraphicsScene;

class IHeightScene {
public:
    virtual ~IHeightScene() = default;
    // 显示图像，返回是否成功
    virtual bool showImage(const QImage& image) = 0;
    virtual void clearRoi() = 0;
protected:
    IHeightScene() = default;
};


class IHeightMeasure {
public:
    virtual ~IHeightMeasure() = default;
    // 加载文件夹中的图片序列
    virtual bool LoadFolder(const QString& path) = 0;
    // 选择单张图片进行测高
    virtual bool SelectImage(const QString& imagePath) = 0;
    // 设置基准图像及对应的已知高度（用于标定）
    virtual bool SetPreference(const QString& preferenceImagePath, double height) = 0;
    // 保存 / 加载标定数据
    virtual bool SaveCalibrationData(const QString& savePath) = 0;
    virtual bool LoadCalibrationData(const QString& loadPath) = 0;
    // 查询当前是否已完成标定
    virtual bool isCalibrated() const = 0;
    // 启动 ROI 交互选择（进入拾取模式）
    virtual bool SelectROI() = 0;
    // 确认当前 ROI 区域
    virtual bool ConfirmROI() = 0;
    // 对当前图像执行测高，返回测量结果（失败返回 -1.0）
    virtual double StartTest() = 0;
    // 获取最近一次的测量结果
    virtual double getMeasurementResult() const = 0;
    // 获取内部 Scene
    virtual IHeightScene* getScene() const = 0;
protected:
    IHeightMeasure() = default;
};

HEIGHTMEATURE_EXPORT IHeightMeasure* newIHeightMeasure();