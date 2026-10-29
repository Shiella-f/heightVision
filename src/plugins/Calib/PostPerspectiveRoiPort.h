#pragma once
#include <QJsonObject>
#include <QPointF>
#include <QSizeF>
#include <QVector>
#include <QString>
#include <opencv2/core.hpp>
#if defined(CalibPLUGIN_LIBRARY)
#define POST_ROI_EXPORT Q_DECL_EXPORT
#else
#define POST_ROI_EXPORT Q_DECL_IMPORT
#endif

// Pixel centers in the UNCROPPED perspective image, TL, TR, BR, BL.
struct PostPerspectiveRoiConfig {
    QVector<QPointF> corners;
    QSizeF referenceSizeMm{299.5,299.5};
    QSizeF targetSizeMm{300,300};
};
struct PostPerspectiveRoiCandidate {
    QVector<QPointF> corners;
    double areaPixels=0;
    double edgeSupport=0;
};
POST_ROI_EXPORT bool CanExtractPostPerspectiveRoi(double widthMm,double heightMm);
POST_ROI_EXPORT bool DetectPostPerspectiveRectangles(const cv::Mat& baseImage,
    const QSizeF& baseSizeMm,const QSizeF& referenceSizeMm,
    QVector<PostPerspectiveRoiCandidate>& candidates,QString& error);
