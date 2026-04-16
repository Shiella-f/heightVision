#pragma once
#include <opencv2/opencv.hpp>
#include <QImage>

namespace TIGER_BSVISION
{
    // 3通道图
    bool qImage2cvImage(const QImage &p_qimg, cv::Mat &p_mat, int channel = 3);
    bool cvImage2qImage(QImage &p_qimg, const cv::Mat &p_mat);
    bool cvImage2qImageGray(QImage &p_qimg, const cv::Mat &p_matGray);
    bool cvImage2qImageRGB(QImage &p_qimg, const cv::Mat &p_matBGR);
    bool cvImage2qImageRGBA(QImage &p_qimg, const cv::Mat &p_matBGRA);

}