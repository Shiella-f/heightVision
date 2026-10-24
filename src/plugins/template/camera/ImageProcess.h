#pragma once
#include <QWidget>
#include <opencv2/opencv.hpp>
#include <QImage>
#include <QMessageBox>
#include <QObject>
#include "../template_global.h"
#include "ImageprocessPort.h"

class ImageProcess : public QObject, public ImageProcessPort{
    Q_OBJECT
    
public:
    ImageProcess();
    
    bool loadCameraCalibration(const QString& calibFilePath) override; // 加载相机去畸变数据
    bool loadTiltCalibration(const QString& calibFilePath) override; // 加载倾斜校正数据
    bool load3x3Calibration(const QString& calibFilePath) override; // 加载3x3标定数据
    bool load9x9Calibration(const QString& calibFilePath) override; // 加载9x9标定数据
    
    cv::Mat CalibImage(cv::Mat &originalImage) override;//去畸变图像，输入原始图像，输出去畸变后的图像
    cv::Mat WarPerspective(cv::Mat& originalImage) override; // 校正图像，输入单应性矩阵，输出校正后的图像
    cv::Mat enhanceResultImage(const cv::Mat& src) override;
    cv::Mat apply3x3Calibration(const cv::Mat& originalImage) override; // 应用3x3数据进行图像裁剪

    bool isCalibrationLoaded() const override{ return m_isCameraCalibLoaded; }
    bool m_isTiltLoaded() const override; // 判断透视变换数据是否加载成功

    double mmPixel() const override; // 获取毫米/像素的转换比例
    bool setCoordinateOriginByPair(cv::Point2d pixelPt,cv::Point2d physicalPtMm) override;
    
    //像素坐标->物理坐标（mm）
    cv::Point2d pixelToMm(cv::Point2d px) override {return { px.x / m_config.pixPerMm + physicalOffsetMm_.x, px.y / m_config.pixPerMm + physicalOffsetMm_.y };}
    //物理坐标->像素坐标
    cv::Point2d mmToPixel(cv::Point2d mm) override {return { (mm.x - physicalOffsetMm_.x) * m_config.pixPerMm, (mm.y - physicalOffsetMm_.y) * m_config.pixPerMm };}
    void loadCalibrationData(); // 加载标定数据
private:
    
    void resetPhysicalOffset();
    cv::Mat m_cameraMatrix;
    cv::Mat m_distCoeffs;
    cv::Mat m_map1, m_map2;
    bool m_isCameraCalibLoaded = false;
    cv::Mat WarPerspectiveMatrix; // 存储单应性矩阵
    cv::Size m_calibImageSize;
    QRect roi_3x3 = QRect(0, 0, 0, 0); // 3x3 ROI，默认中心区域

    cv::Mat HomographyMatrix; // 用于存储单应性矩阵
    bool m_is9_9CalibLoaded = false;
    bool m_is3_3CalibLoaded = false;

    struct TiltParams {
    cv::Mat homography;     //透视变换矩阵 H
    bool hasMap_ = false;
    };
    TiltParams m_TiltParams; // 存储倾斜校正参数
    cv::Point2d physicalOffsetMm_ = {0.0, 0.0}; // 物理坐标偏移量，单位：mm
    struct Config
    {
        // 棋盘格（格子数，非内角点数）
        int    gridCols    = 12;
        int    gridRows    = 9;
        double squareMm    = 6.0;

        // 输出图像的物理范围（mm）
        double outWidthMm  = 110.0;
        double outHeightMm = 110.0;

        // 输出分辨率
        double pixPerMm    = 10.0;   // 10 px/mm → 1100×1100 px

        // 内角点数
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
    Config m_config; // 存储标定配置参数

};

