#pragma once
#include <opencv2/opencv.hpp>

class IImageProcess {
public:
    virtual ~IImageProcess() = default;

    virtual void loadCalibrationData() = 0; // 加载标定数据
    virtual cv::Mat CalibImage(cv::Mat &originalImage) = 0; // 去畸变处理，输入原图，输出处理后的图像
    virtual bool isCalibrationLoaded() const = 0;
    virtual cv::Mat WarPerspective(cv::Mat& originalImage) = 0; // 校正图像，输入单应性矩阵，输出校正后的图像
    virtual bool m_isTiltLoaded() const = 0; // 判断透视变换数据是否加载成功
    virtual double mmPixel() const = 0; // 获取毫米/像素的转换比例;正确返回double 否则返回-1;使用前先标定
    virtual bool setCoordinateOriginByPair(cv::Point2d pixelPt,cv::Point2d physicalPtMm) = 0;//设置偏移量
    virtual void resetPhysicalOffset() = 0;//重置偏移量为0
    virtual cv::Point2d pixelToMm(cv::Point2d px) const = 0;//像素坐标->物理坐标(mm)；请先设置偏移量
    virtual cv::Point2d mmToPixel(cv::Point2d mm) const = 0;//物理坐标(mm)->像素坐标；请先设置偏移量

};

// 声明工厂函数类型 (如果需要动态加载)
typedef IImageProcess* (*CreateImageProcessFunc)();