# pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <QImage>
#include <QPixmap>
#include <QPainterPath>

namespace bscvTool1 
{
    // 将Qt的QImage转换为OpenCV的cv::Mat
    bool qImage2cvImg(const QImage& inImg, cv::Mat& outImg, int channels = 3);
    //将OpenCV的cv::Mat转换为Qt的QImage（通用转换，根据Mat的通道自动处理）
    bool cvImg2QImage(const cv::Mat& inImg, QImage& outImg);
    //将OpenCV的灰度图（cv::Mat）转换为Qt的QImage（灰度格式）
    bool cvImg2qImg_Gray(const cv::Mat& inImg, QImage& outImg);
    //将OpenCV的BGR格式图像转换为Qt的RGB格式QImage
    bool cvImg2qImg_RGB(const cv::Mat& inImg, QImage& outImg);
    //将OpenCV的BGRA格式图像转换为Qt的RGBA格式QImage
    bool cvImg2qImg_RGBA(const cv::Mat& inImg, QImage& outImg);
    //将Qt的QPainterPath描述的区域转换为OpenCV的掩码矩阵（cv::Mat）
    bool qRegion2cvRegion(const QSize &p_regionsize, const QPainterPath& p_path, cv::Mat& outRegion);
    //将Qt的QPainterPath描述的区域转换为OpenCV的掩码矩阵，并获取区域的最小包围矩形
    bool qRegion2cvMinRegion(const QSize &p_regionsize, const QPainterPath& p_path, cv::Mat& outRegion, cv::Rect& outRect);
}