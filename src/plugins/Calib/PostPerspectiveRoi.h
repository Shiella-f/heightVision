#pragma once
#include "PostPerspectiveRoiPort.h"

namespace calibration {
// Optional, fixed transform; detection is never performed by a frame consumer.
struct PostPerspectiveRoi {
    bool enabled=false;
    PostPerspectiveRoiConfig config;
    cv::Size baseSize,outputSize;
    cv::Size2d baseSizeMm;
    double density=0;
    cv::Mat basePixelToMm,baseMmToRoiMm;
    QString binding;
    bool configure(const PostPerspectiveRoiConfig& requested,cv::Size pixels,cv::Size2d mm,
        double pixelsPerMm,const cv::Mat& pixelToMm,const QString& baseBinding,QString& error);
    bool apply(const cv::Mat& source,cv::Mat& output)const;
    bool map(cv::Point2d& baseMm)const;
    QJsonObject report()const;
    void write(cv::FileStorage& fs)const;
    bool read(const cv::FileNode& node,cv::Size pixels,cv::Size2d mm,double pixelsPerMm,
        const cv::Mat& pixelToMm,const QString& baseBinding,QString& error);
};
cv::Mat halfPixelToMm(cv::Size pixels,cv::Size2d mm);
}
