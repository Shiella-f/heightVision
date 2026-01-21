/**
 * @file   hough.h
 * @brief  实现hough圆特征识别。
 * @author 鄢然
 * @date   2024-04-19
 */
#ifndef BSCV_MODULES_IMGPROC_INCLUDE_HOUGH_H
#define BSCV_MODULES_IMGPROC_INCLUDE_HOUGH_H

#include "opencv2/core.hpp"

namespace TIGER_BSVISION
{
#define RadiusStep 1  //半径搜索时的步长，越小精度越高，效率越低。

    // struct CHoughCircleMask
    // {
    //     double thresh = 80;  // 二值化阈值
    //     cv::Point kernelSize = cv::Size(5, 5);  // 腐蚀膨胀核大小
    //     double contourAreaMax = 1000;  // 连通域过滤面积
    //     double outCircleRadioMax = 1.2;
    //     double outCircleRadioMin = 0.8;
    //     double innerCircleRadioMax = 1.1;
    //     double innerCircleRadioMin = 0.7;
    // };

    struct CHoughCircleMask
    {
        double thresh = 80;  // 二值化阈值
        cv::Point kernelSize = cv::Size(10, 10);  // 腐蚀膨胀核大小
        double contourAreaMax = 1000;  // 连通域过滤面积
        // double outCircleRadioMax = 1.2;
        // double outCircleRadioMin = 0.8;
        double outCircleRadioMax = 1.1;
        double outCircleRadioMin = 0.9;
        double innerCircleRadioMax = 1.1;
        double innerCircleRadioMin = 0.7;
    };

    class IHoughCircle
    {
    public:
        virtual ~IHoughCircle() = default;

        /*
            image：输入图像，它应该是一个8位的，单通道图像；
            _templ：模板图像，用于初始定位的图像；
            timeout: 算法超时时间；
            second: 是否开启二次识别，默认开启，开启后精度会提高；
            outContour: true识别外侧圆，false识别内测圆；
            minRadius: 最小半径；
            maxRadius: 最大半径。
        */
        virtual void init(cv::InputArray _image, cv::InputArray _templ, int _radius, float _minDist = 0, int _maxCircles = 1, int _timeout = 2000) = 0;
        virtual void init(cv::InputArray _image, int _timeout = 2000, bool _second = true, bool _outContour = false, int _minRadius = 0, int _maxRadius = 0, float _minDist = 0, int _maxCircles = 1) = 0;
        virtual void initMinMax(cv::InputArray _image, cv::InputArray _templ, int _timeout = 2000, bool _second = true, bool _outContour = false, int _minRadius = 0, int _maxRadius = 0, float _minDist = 0, int _maxCircles = 1) = 0;
        virtual void process(cv::InputArray _image, bool _second = true, bool _outContour = false, int _minRadius = 0, int _maxRadius = 0, float _minDist = 0, int _maxCircles = 1) = 0;
        virtual void openMask(bool _open) = 0;  //是否开启图形预处理
        virtual void setMaskPara(CHoughCircleMask _para) = 0;

        virtual bool bInit() const = 0;
        virtual cv::Vec3f circle() const = 0;
        virtual cv::Vec3f circleAvg() const = 0;
        virtual cv::Vec3f circleMin() const = 0;
        virtual cv::Vec3f circleMax() const = 0;
        virtual std::vector<cv::Vec3f> circles() const = 0;
        virtual int accThreshold() const = 0;
        virtual int accum() const = 0;

    protected:
        IHoughCircle() = default;
    };

    std::unique_ptr<IHoughCircle> getHoughCircle();
    IHoughCircle* newHoughCircle();
};

#endif  // BSCV_MODULES_IMGPROC_INCLUDE_HOUGH_H