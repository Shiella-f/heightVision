#pragma once
#include <QString>
#include <QImage>
#include <opencv2/opencv.hpp>
#include <QWidget>
#include <QtCore/qglobal.h>

#ifndef CAMERAWARPTIVEPORT_EXPORT
#  if defined(CalibPLUGIN_LIBRARY)
#    define CAMERAWARPTIVEPORT_EXPORT Q_DECL_EXPORT
#  else
#    define CAMERAWARPTIVEPORT_EXPORT Q_DECL_IMPORT
#  endif
#endif

class ICameraWarptiveSeene{
public:
    virtual ~ICameraWarptiveSeene() = default;
    virtual void setOriPixmap(const QPixmap& pixmap) = 0; // 设置原始图像
protected:
    ICameraWarptiveSeene() = default;
};

class CameraWarptivePort{
public:

    struct Config
    {
        // 棋盘格（格子数，非内角点数）
        int    gridCols    = 12;
        int    gridRows    = 9;
        double squareMm    = 6.0;

        // 输出图像的物理范围（mm），须 >= 标定板物理尺寸
        double outWidthMm  = 110.0;
        double outHeightMm = 110.0;

        // 输出分辨率
        double pixPerMm    = 10.0;   // 10 px/mm → 1100×1100 px

        // 角点数
        int innerCols() const { return gridCols - 1; }
        int innerRows() const { return gridRows - 1; }

        // 标定板在输出图中的居中偏移（mm）
        double marginXmm() const { return (outWidthMm  - gridCols * squareMm) / 2.0; }
        double marginYmm() const { return (outHeightMm - gridRows * squareMm) / 2.0; }

        // 输出图像像素尺寸
        cv::Size outSize() const
        {
            return { (int)std::round(outWidthMm * pixPerMm),
                     (int)std::round(outHeightMm * pixPerMm) };
        }
    };


    virtual ~CameraWarptivePort() = default;
    virtual bool CalibWarptive(const Config& config, const cv::Mat& srcImage) = 0;//棋盘格仿射变换
    virtual bool UseWarptiveImage(const cv::Mat& srcImage, cv::Mat& dstImage) const = 0; // 获取校正后的图像
    virtual bool SaveResult(const QString savePath) = 0;// 保存标定结果,后缀使用.XML或者.YAML
    virtual ICameraWarptiveSeene* getScene() const = 0;//返回场景指针
protected:
    CameraWarptivePort() = default;
};

CAMERAWARPTIVEPORT_EXPORT CameraWarptivePort* newCameraWarptivePort();