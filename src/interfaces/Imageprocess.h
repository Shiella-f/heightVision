#pragma once
#include <opencv2/opencv.hpp>

class IImageProcess {
public:
    virtual ~IImageProcess() = default;

    virtual void loadCalibrationData() = 0; // 加载标定数据
    virtual cv::Mat CalibImage(cv::Mat &originalImage) = 0; // 图像校准
    virtual bool isCalibrationLoaded() const = 0;

};

// 声明工厂函数类型 (如果需要动态加载)
typedef IImageProcess* (*CreateImageProcessFunc)();