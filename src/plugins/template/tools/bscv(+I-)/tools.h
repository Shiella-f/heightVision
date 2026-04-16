#ifndef BSCV_MODULES_TOOLS_INCLUDE_TOOLS_H
#define BSCV_MODULES_TOOLS_INCLUDE_TOOLS_H

#include "opencv2/core.hpp"

namespace TIGER_BSVISION
{
    void ShowCircles(std::vector<cv::Vec3f> p_circles, cv::Mat p_colorImg, cv::String p_name = "");
    void ShowCircle(cv::Vec3f p_circle, cv::Mat p_colorImg, cv::String p_name = "");

    // 返回一组圆的最大圆心偏差。
    cv::Vec2f maxDeviation(std::vector<cv::Vec3f> p_circles);

    // 把图片的ROI轮廓保存为新的图片。
    void saveROI(cv::String p_imageName, cv::Rect p_rect);
};

#endif  //BSCV_MODULES_TOOLS_INCLUDE_TOOLS_H