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
    // 加载独立 YAML 参数包。失败时使旧参数和旧结果失效。
    virtual bool LoadCalibrationData(const QString& path) = 0;
    // gridSize: 方阵行列数，允许 3、5、7……65。支持 CV_8UC1/3/4。
    // 输入相机原图；须先加载有效参数包，图像尺寸须与参数包一致。
    // 每次调用清除上次结果；只有完整点阵与规格一致且坐标换算成功时返回 true。
    virtual bool Calibrate(const cv::Mat& image, int gridSize) = 0;
    virtual bool Calibrate(const QString& imagePath, int gridSize) = 0;
    // 原图像素坐标：左上原点，X 向右、Y 向下，按网格逐行、行内从左到右排序。
    virtual QVector<QPointF> GetPoints() const = 0;
    // 单位 mm，沿用透视标定的原点和方向，与 GetPoints() 逐点对应。
    virtual QVector<QPointF> GetPhysicalPoints() const = 0;
    // CSV: Index,PixelX,PixelY,PhysicalXmm,PhysicalYmm；保留六位小数。
    virtual bool SaveResult(const QString& savePath) const = 0;

    virtual IMirrorScene* getScene() const = 0;//返回场景指针
protected:
    CalibWarptivePort() = default;
};

MIRRORCALIBPORT_EXPORT CalibWarptivePort* newCalibWarptivePort();

