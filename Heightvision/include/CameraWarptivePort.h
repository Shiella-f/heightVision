#pragma once
#include <QString>
#include <QImage>
#include <opencv2/opencv.hpp>
#include <QWidget>
#include <QJsonObject>
#include <QPointF>
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
    enum class BoardType { Chessboard = 0, Charuco = 1 };
    enum class InputStage { Raw = 0, Undistorted = 1 };

    struct Config
    {
        // 棋盘格（格子数，非内角点数）
        int    gridCols    = 12;
        int    gridRows    = 9;
        double squareMm    = 6.0;

        // 输出图像的物理范围（mm），须 >= 标定板物理尺寸
        double outWidthMm  = 130.0;
        double outHeightMm = 130.0;

        // 输出分辨率。标定时会根据检测到的棋盘格角点自适应更新该值。
        double pixPerMm    = 10.0;

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

        // 新版 Config 与 DLL 必须配套使用；ChArUco 使用实际方格和标记边长。
        BoardType boardType = BoardType::Chessboard;
        double markerMm = 0.0;
        int dictionaryId = 0; // DICT_4X4_50
        // 已去畸变图必须使用所加载参数的 K_new，保持原分辨率且未裁剪/缩放。
        InputStage inputStage = InputStage::Raw;
        double fitToleranceCells = 0.03; // 单格边长为 1，允许 0.001～0.10
        QJsonObject boardConfiguration; // nonempty: complete authoritative board JSON
        bool useOutputRegion = false; // opt in; legacy output behavior otherwise unchanged
    };


    virtual ~CameraWarptivePort() = default;
    // 须先加载相机参数；按 config.inputStage 接收同分辨率原图或已去畸变图。
    virtual bool CalibWarptive(const Config& config, const cv::Mat& srcImage) = 0;
    // CV_8UC1/3/4，输入阶段必须与本次 CalibWarptive 的 Config 一致。
    // 原图合并去畸变和透视为一次采样并复用映射表；已去畸变图只应用 H。
    // 支持输入输出为同一个 Mat；失败时清空 dstImage。
    virtual bool UseWarptiveImage(const cv::Mat& srcImage, cv::Mat& dstImage) const = 0;
    virtual bool SaveResult(const QString savePath) = 0;// 保存标定结果,后缀使用.XML或者.YAML
    virtual ICameraWarptiveSeene* getScene() const = 0;//返回场景指针
    virtual bool LoadCameraCalibrationData(const QString& path) = 0;
    virtual bool SaveMeasurementCalibration(const QString& path) const = 0;
    virtual bool MapInputToBoard(const QPointF& input,QPointF& boardMm) const = 0;
    virtual bool SetOutputRegion(const QPointF& centerMm,double widthMm,double heightMm,double pixelsPerMm) = 0;
    virtual QJsonObject GetReport() const = 0;
protected:
    CameraWarptivePort() = default;
};

CAMERAWARPTIVEPORT_EXPORT CameraWarptivePort* newCameraWarptivePort();
