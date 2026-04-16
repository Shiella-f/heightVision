#pragma once
#include <QString>
#include <QRectF>
#include <QImage>
#include <opencv2/opencv.hpp>

class ImageProcessPort{
    public:
    virtual ~ImageProcessPort() = default;
    virtual bool loadCameraCalibration(const QString& calibFilePath) = 0; // 加载相机去畸变数据
    virtual bool loadTiltCalibration(const QString& calibFilePath) = 0; // 加载倾斜校正数据
    virtual bool load3x3Calibration(const QString& calibFilePath) = 0; // 加载3x3标定数据
    virtual bool load9x9Calibration(const QString& calibFilePath) = 0; // 加载9x9标定数据
    virtual cv::Mat CalibImage(cv::Mat &originalImage) = 0;//去畸变图像，输入原始图像，输出去畸变后的图像
    virtual cv::Mat WarPerspective(cv::Mat& originalImage) = 0; // 校正图像，输入单应性矩阵，输出校正后的图像
    virtual cv::Mat apply3x3Calibration(const cv::Mat& originalImage) = 0; // 应用3x3数据进行图像裁剪
    virtual bool isCalibrationLoaded() const = 0;// 判断相机标定数据是否加载成功
    virtual bool m_isTiltLoaded() const = 0; // 判断透视变换数据是否加载成功
    virtual double mmPixel() const = 0; // 获取毫米/像素的转换比例
    virtual bool setCoordinateOriginByPair(cv::Point2d pixelPt,cv::Point2d physicalPtMm) = 0;// 设置坐标原点，输入像素坐标和对应的物理坐标（mm）
    
    //像素坐标->物理坐标（mm）
    virtual cv::Point2d pixelToMm(cv::Point2d px) = 0;
    //物理坐标->像素坐标
    virtual cv::Point2d mmToPixel(cv::Point2d mm) = 0;
protected:
    ImageProcessPort() = default;
};

ImageProcessPort* newImageProcessPort();

