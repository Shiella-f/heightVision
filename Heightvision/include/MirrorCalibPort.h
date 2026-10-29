#pragma once
#include <QString>
#include <QImage>
#include <QPointF>
#include <QPoint>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <QWidget>
#include <QtCore/qglobal.h>

#ifndef MIRRORCALIBPORT_EXPORT
#  if defined(CalibPLUGIN_LIBRARY)
#    define MIRRORCALIBPORT_EXPORT Q_DECL_EXPORT
#  else
#    define MIRRORCALIBPORT_EXPORT Q_DECL_IMPORT
#  endif
#endif

class IMirrorScene{
public:
    virtual ~IMirrorScene() = default;
    virtual void setOriPixmap(const QPixmap& pixmap) = 0; // 设置原始图像
protected:
    IMirrorScene() = default;
};

class CalibWarptivePort{
public:
    virtual ~CalibWarptivePort() = default;
    // 加载独立 YAML 基础参数，完全忽略新增 post_perspective_roi（含其校验）。
    // 保留基础输出范围、有效域、去畸变及透视映射；失败时旧参数和旧结果失效。
    virtual bool LoadCalibrationData(const QString& path) = 0;
    // gridSize: 方阵行列数，允许 3、5、7……65。支持 CV_8UC1/3/4。
    // 输入相机原图；尺寸须与参数包一致。先生成完整基础透视图，再提取点阵，不应用新增 ROI。
    // 每次调用清除上次结果；只有完整点阵与规格一致且坐标换算成功时返回 true。
    virtual bool Calibrate(const cv::Mat& image, int gridSize) = 0;
    virtual bool Calibrate(const QString& imagePath, int gridSize) = 0;
    // 基础透视图像素坐标（不是原图像素）：左上原点，X 向右、Y 向下，逐行从左到右。
    virtual QVector<QPointF> GetPoints() const = 0;
    // 单位 mm，沿用基础透视原点、方向及像素采样约定，不应用新增 ROI，与 GetPoints() 对应。
    virtual QVector<QPointF> GetPhysicalPoints() const = 0;
    // CSV: Index,PixelX,PixelY,PhysicalXmm,PhysicalYmm；Pixel 为透视像素，保留六位小数。
    virtual bool SaveResult(const QString& savePath) const = 0;

    virtual IMirrorScene* getScene() const = 0;//返回场景指针
protected:
    CalibWarptivePort() = default;
};

MIRRORCALIBPORT_EXPORT CalibWarptivePort* newCalibWarptivePort();

