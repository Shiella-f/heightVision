#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <stdexcept>
#include "../Function/CameraWarptivePort.h"

class CameraCorrector {
public:
    using Config = CameraWarptivePort::Config;

    CameraCorrector(const Config& cfg = Config{});
    ~CameraCorrector() = default;
    bool computeMap(const cv::Mat& undistImg);
    bool warp(const cv::Mat& undistImg, cv::Mat& dst) const;

    // 直接提供单应矩阵
    void setHomography(const cv::Mat& H) { H_ = H.clone(); hasMap_ = !H_.empty(); }
    cv::Mat homography() const { return H_.clone(); }

    bool    ready()      const { return hasMap_; }
    Config  config()     const { return cfg_; }

    // mm/px，用于量测
    double mmPerPixel() const { return 1.0 / cfg_.pixPerMm; }

    // 二次初始化：调整坐标系平移
    bool setCoordinateOriginByPair(cv::Point2d pixelPt,
                                   cv::Point2d physicalPtMm);
    void setPhysicalOffsetMm(cv::Point2d offsetMm) { physicalOffsetMm_ = offsetMm; }
    void resetPhysicalOffset() { physicalOffsetMm_ = {0.0, 0.0}; }
    cv::Point2d physicalOffsetMm() const { return physicalOffsetMm_; }

    

    bool save(const std::string& path) const;
    bool load(const std::string& path);

    //将输出图像上的像素坐标换算为物理坐标（mm）。
    cv::Point2d pixelToMm(cv::Point2d px) const{return { px.x / cfg_.pixPerMm + physicalOffsetMm_.x, px.y / cfg_.pixPerMm + physicalOffsetMm_.y };}

    //物理坐标（mm）换算为输出图像素坐标。
    cv::Point2d mmToPixel(cv::Point2d mm) const{return { (mm.x - physicalOffsetMm_.x) * cfg_.pixPerMm, (mm.y - physicalOffsetMm_.y) * cfg_.pixPerMm };}

private:
    Config  cfg_;
    cv::Mat H_;        //单应矩阵
    bool    hasMap_ = false;
    cv::Point2d physicalOffsetMm_ = {0.0, 0.0}; // 二次初始化得到的物理坐标偏移

    //理想角点位置（单位：像素）
    std::vector<cv::Point2f> makeTargetPts() const;

    // 在图像中检测内角点（亚像素）
    bool detectCorners(const cv::Mat& img,
                       std::vector<cv::Point2f>& corners) const;
};