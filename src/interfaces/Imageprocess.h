#pragma once
#include <opencv2/opencv.hpp>

class IImageProcess {
public:
    virtual ~IImageProcess() = default;

    virtual void loadCalibrationData() = 0; // 加载标定数据
    //virtual void loadWarPerspectiveData() = 0; // 加载单应性矩阵数据
    virtual cv::Mat CalibImage(cv::Mat &originalImage) = 0; // 去畸变处理，输入原图，输出处理后的图像
    //virtual cv::Mat WarPerspective(cv::Mat& originalImage) = 0; // 校正图像，输入单应性矩阵，输出校正后的图像
    virtual bool isCalibrationLoaded() const = 0;

};

// 声明工厂函数类型 (如果需要动态加载)
typedef IImageProcess* (*CreateImageProcessFunc)();