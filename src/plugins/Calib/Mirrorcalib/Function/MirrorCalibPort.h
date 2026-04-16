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
    virtual bool Calibrate3x3(const cv::Mat& image) = 0;//3x3标定
    virtual bool Calibrate9x9(const cv::Mat& image) = 0;//9x9标定
    virtual bool Save3x3Result(const QString savePath) = 0;// 保存3x3标定结果
    virtual QVector<QPointF> Get9x9Points() const = 0; // 获取9x9点坐标
    //virtual bool Save9x9Result(const QString savePath) = 0;// 保存9x9标定结果

    virtual IMirrorScene* getScene() const = 0;//返回场景指针
protected:
    CalibWarptivePort() = default;
};

MIRRORCALIBPORT_EXPORT CalibWarptivePort* newCalibWarptivePort();

