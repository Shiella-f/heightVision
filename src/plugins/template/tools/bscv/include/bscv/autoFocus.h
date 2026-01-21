#ifndef BSCV_AutoFocus_H
#define BSCV_AutoFocus_H
#include "opencv2/opencv.hpp"

namespace TIGER_BSVISION
{
/*
    计算出的值越大说明图片越清晰，图片应为拍照的图片的中间区域，roi区域一般为整体大小的八分之一。
    image：输入图像，它应该是一个8位的，单通道图像。
*/
    double calculateFocusMeasure(const cv::Mat& _image);
}
#endif
