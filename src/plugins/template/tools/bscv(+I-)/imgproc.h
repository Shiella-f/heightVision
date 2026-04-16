#ifndef BSCV_MODULES_IMGPROC_INCLUDE_IMGPROC_H
#define BSCV_MODULES_IMGPROC_INCLUDE_IMGPROC_H

#include "opencv2/core.hpp"

namespace TIGER_BSVISION
{
/*
    image：输入图像，它应该是一个8位的，单通道图像。
    circles：输出向量，它将存储检测到的圆的输出结果。每个检测到的圆由三个元素的浮点向量表示 (x, y, radius)。
    minDist：检测到的圆的中心之间的最小距离。如果参数太小，多个相邻的圆可能被错误地检测为一个重叠的圆。反之，如果参数太大，某些圆可能无法被检测。
    param1：canny边缘检测器的更高阈值（较低的阈值是两倍小），默认值为100，设置为0时为自动梯度，在处理比较模糊的图片时设置为0。
    param2：圆心检测阶段的圆心累加器阈值，代表在圆周上像素点的数量。较小的参数可能会使更多的假圆被检测。较大的参数会只检测那些更加明确的圆，同时导致小圆被漏掉。默认值为100。推荐值为100，75，50，30，20。图片比较模糊时，推荐依次取较小值。
    param3：代表圆心累计器值除以圆半径的最小值，低于该值的圆会被过滤掉，减少小圆检测时误检出低累加器值得大圆，图片叫模糊时推荐取0.
    minRadius和maxRadius：检测的圆的最小和最大半径。默认情况下，它们被设置为0意味着只要可能，都会被视为有效圆。半径指定的范围越小，效率越高。
    检测较大圆，并且图片较清晰时，推荐使用HoughCircles(grayImage, circles, grayImage.rows / 8, 100, 100, 1.5, 0, 0);
    需要检测小圆的情况下，推荐使用HoughCircles(grayImage, circles, grayImage.rows / 20, 100, 30, 2, 0, 0);
    需要检测较为模糊的图片时，推荐使用HoughCircles(grayImage, circles, grayImage.rows / 20, 0, 30, 0, 0, 0);
    需要检测指定半径的圆时，推荐使用HoughCircles(grayImage, circles, grayImage.rows / 8, 100, 50, 0, radius * 0.9, radius * 1.1)，
    需要检测指定半径的圆时，图片较为模糊时HoughCircles(grayImage, circles, grayImage.rows / 8, 0, 30, 0, radius * 0.9, radius * 1.1)
*/
void HoughCircles(cv::InputArray _image, cv::OutputArray _circles,
                        double minDist,
                        double param1, double param2, double param3,
                        int minRadius, int maxRadius);

/*
    提供一个简单的模板匹配工具函数，采用cv::TM_CCORR_NORMED匹配方法。
    返回_templ在_image里面的最大匹配值位置。
*/
cv::Point matchTemplate(cv::InputArray _image, cv::InputArray _templ);

double CannyValue(cv::InputArray _image, double param1);

double getMaxGrayValue(cv::InputArray _image, cv::InputArray _mask = cv::Mat());

double getMeanGrayValue(cv::InputArray _image, cv::InputArray _mask = cv::Mat());

//void Hough();
};

#endif  // BSCV_MODULES_IMGPROC_INCLUDE_IMGPROC_H