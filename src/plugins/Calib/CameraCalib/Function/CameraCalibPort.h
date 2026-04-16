#pragma once
#include <QString>
#include <QImage>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <QtCore/qglobal.h>

#ifndef CAMERACALIBPORT_EXPORT
#  if defined(CalibPLUGIN_LIBRARY)
#    define CAMERACALIBPORT_EXPORT Q_DECL_EXPORT
#  else
#    define CAMERACALIBPORT_EXPORT Q_DECL_IMPORT
#  endif
#endif

class ICameraCalibScene{
public:
    virtual ~ICameraCalibScene() = default;
    virtual void setOriPixmap(const QPixmap& pixmap) = 0; // 设置原始图像
protected:
    ICameraCalibScene() = default;
};

class CameraCalibPort{
public:
    struct CalibData
    {
        cv::Mat cameraMatrix; // 相机内参矩阵
        cv::Mat distCoeffs;   // 畸变系数
        std::vector<cv::Mat> rvecs; // 旋转向量
        std::vector<cv::Mat> tvecs; // 平移向量
        double rms;           // 重投影误差
    };
    virtual ~CameraCalibPort() = default;
    /**
     * @brief Calibrate 进行相机标定
     * @param imagePaths 输入的图片路径列表
     * @param boardSize 棋盘格内角点的行列数 (width, height)
     * @param squareSize 方格尺寸 (mm)
     * @param useCharuco 是否使用 ChArUco 标定
     * @param markerSize ChArUco 中标记的尺寸 (mm)，仅在 useCharuco=true 时有效
     * @param dictionaryId ArUco 字典 ID，默认使用 DICT_4X4_50，仅在 useCharuco=true 时有效
     * @return 标定是否成功
     */
    virtual bool Calibrate(QVector<QString> imagePaths, cv::Size boardSize, float squareSize, 
                            bool useCharuco = false, const float markerSize = 0.0f, 
                            const int dictionaryId = -1) = 0;//暂时只支持棋盘格标定
    virtual bool SaveResult(const QString savePath) = 0;// 保存标定结果,后缀使用.XML或者.YAML
    virtual CalibData getCalibData() const = 0; // 获取标定结果数据
    virtual bool isCalibrated() const = 0; // 是否已完成标定
    virtual ICameraCalibScene* getScene() const = 0;//返回场景指针
protected:
    CameraCalibPort() = default;
};

CAMERACALIBPORT_EXPORT CameraCalibPort* newCameraCalibPort();