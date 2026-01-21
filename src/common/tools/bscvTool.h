#pragma once
#include <opencv2/opencv.hpp>
#include <bscv/templmatch.h>
#include <QPainterPath>
#include <QImage>

namespace TIGER_BSVISION
{
    // 3通道图
    bool qImage2cvImage(const QImage &p_qimg, cv::Mat &p_mat, int channel = 3);
    bool cvImage2qImage(QImage &p_qimg, const cv::Mat &p_mat);
    bool cvImage2qImageGray(QImage &p_qimg, const cv::Mat &p_matGray);
    bool cvImage2qImageRGB(QImage &p_qimg, const cv::Mat &p_matBGR);
    bool cvImage2qImageRGBA(QImage &p_qimg, const cv::Mat &p_matBGRA);
    bool qRegion2cvRegion(const QSize &p_regionSize, const QPainterPath &p_path, cv::Mat &p_hRegion);
    bool qRegion2MinRegion(const QSize &p_regionSize, const QPainterPath &p_path, cv::Mat &p_hRegion, cv::Rect &p_boundingRect);
    cv::RotatedRect getSmallestRectangle(const cv::Mat &p_matRegion);

    cv::Mat getSmallestRectangleRegion(const cv::Mat &p_matRegion, bool p_fill = false);
    cv::Mat getSmallestEllipseRegion(const cv::Mat &p_matRegion, bool p_fill = false);
    cv::Mat getCrossContours(const QSize &p_size, const double &p_rows, const double &p_cols);

    cv::Mat shapeToRegion(const TIGER_BSVISION::Template p_templ, QPointF pStart_row_col, QPoint p_shapeOriginSize_row_col);
    QVector<QPointF> getCirclePts(int p_xc, int p_yc, int radius);
}