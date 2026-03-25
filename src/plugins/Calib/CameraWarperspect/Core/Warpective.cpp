
#include "Warpective.h"
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <QDebug>

CameraCorrector::CameraCorrector(const Config& cfg) : cfg_(cfg)
{
    if (cfg_.innerCols() < 2 || cfg_.innerRows() < 2)
        throw std::invalid_argument("棋盘格至少需要 3×3 格子");
    if (cfg_.squareMm <= 0)
        throw std::invalid_argument("方格尺寸须为正数");
    if (cfg_.outWidthMm  < cfg_.gridCols * cfg_.squareMm ||
        cfg_.outHeightMm < cfg_.gridRows * cfg_.squareMm)
        throw std::invalid_argument("输出尺寸小于标定板物理尺寸");
}


std::vector<cv::Point2f> CameraCorrector::makeTargetPts() const
{
    const double mx = cfg_.marginXmm() * cfg_.pixPerMm;
    const double my = cfg_.marginYmm() * cfg_.pixPerMm;
    const double sp = cfg_.squareMm   * cfg_.pixPerMm;

    std::vector<cv::Point2f> pts;
    pts.reserve(cfg_.innerCols() * cfg_.innerRows());

    for (int r = 0; r < cfg_.innerRows(); ++r)
        for (int c = 0; c < cfg_.innerCols(); ++c)
            pts.emplace_back(
                (float)(mx + (c + 1) * sp),
                (float)(my + (r + 1) * sp));

    return pts;
}

bool CameraCorrector::detectCorners(const cv::Mat& img,
                                    std::vector<cv::Point2f>& corners) const
{
    cv::Mat gray;
    if (img.channels() > 1)
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    else
        gray = img;

    const cv::Size boardSz(cfg_.innerCols(), cfg_.innerRows());

    bool found = cv::findChessboardCorners(
        gray, boardSz, corners,
        cv::CALIB_CB_ADAPTIVE_THRESH |
        cv::CALIB_CB_NORMALIZE_IMAGE);

    if (!found)
        return false;

    // 亚像素精化
    cv::cornerSubPix(
        gray, corners,
        cv::Size(11, 11), cv::Size(-1, -1),
        cv::TermCriteria(
            cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER,
            50, 1e-4));

    return (int)corners.size() == cfg_.innerCols() * cfg_.innerRows();
}

bool CameraCorrector::computeMap(const cv::Mat& undistImg)
{
    if (undistImg.empty())
    {
        qDebug() << "[CameraCorrector] 输入图像为空\n";
        return false;
    }

    // 检测所有内角点
    std::vector<cv::Point2f> detectedPts;
    if (!detectCorners(undistImg, detectedPts))
    {
        qDebug() << "[CameraCorrector] 棋盘格检测失败，"
                 << "请确保图像已去畸变且棋盘格完整可见。\n";
        return false;
    }

    //生成目标点
    std::vector<cv::Point2f> targetPts = makeTargetPts();
    CV_Assert(detectedPts.size() == targetPts.size());

    qDebug() << "[CameraCorrector] 检测到 " << detectedPts.size()
             << " 个内角点\n";

    // RANSAC 全角点单应估计
    cv::Mat mask;
    H_ = cv::findHomography(
        detectedPts, targetPts,
        cv::RANSAC,
        1.5,    // 重投影误差阈值 (px)
        mask,
        2000,   // 最大迭代次数
        0.9999  // 置信度
    );

    if (H_.empty())
    {
        qDebug() << "[CameraCorrector] 单应矩阵估计失败\n";
        return false;
    }

    const int inliers = cv::countNonZero(mask);
    const int total   = (int)detectedPts.size();
    const double inlierRatio = (double)inliers / total;

    qDebug() << "[CameraCorrector] RANSAC 内点: "
             << inliers << "/" << total
              << " (" << (int)(inlierRatio * 100) << "%)\n";

    if (inlierRatio < 0.80)
    {
        qDebug() << "[CameraCorrector] 内点比例过低，"
                 << "请检查图像质量或标定板平整度。\n";
        H_.release();
        return false;
    }

    // 计算所有点（含离群点）的重投影误差，用于诊断
    std::vector<cv::Point2f> projPts;
    cv::perspectiveTransform(detectedPts, projPts, H_);

    double sumErr = 0.0, maxErr = 0.0;
    for (int i = 0; i < total; ++i)
    {
        double dx = projPts[i].x - targetPts[i].x;
        double dy = projPts[i].y - targetPts[i].y;
        double e  = std::sqrt(dx*dx + dy*dy);
        sumErr += e;
        maxErr = std::max(maxErr, e);
    }
    const double meanErr    = sumErr / total;
    const double meanErrMm  = meanErr  / cfg_.pixPerMm;
    const double maxErrMm   = maxErr   / cfg_.pixPerMm;
    qDebug() << "[CameraCorrector] 重投影误差：";
    qDebug() << "  均值：" << QString::number(meanErr, 'f', 3) << "px"
         << QString::number(meanErrMm, 'f', 3) << "mm";
    qDebug() << "  最大：" << QString::number(maxErr, 'f', 3) << "px"
         << QString::number(maxErrMm, 'f', 3) << "mm";

    if (meanErr > 2.0)
        qDebug() << "[CameraCorrector]   均值误差 > 2px，"
                  << "建议检查标定板是否严格平面。\n";

    hasMap_ = true;

    cv::Size outSz = cfg_.outSize();
    qDebug() << "[CameraCorrector] 输出尺寸: "
              << outSz.width << "×" << outSz.height << " px, "
              << cfg_.pixPerMm << " px/mm\n";
    qDebug() << "[CameraCorrector] 标定板偏移: ("
              << cfg_.marginXmm() << ", "
              << cfg_.marginYmm() << ") mm\n";

    return true;
}

bool CameraCorrector::warp(const cv::Mat& undistImg, cv::Mat& dst) const
{
    if (!hasMap_)
    {
        qDebug() << "[CameraCorrector] 请先调用 computeMap()。\n";
        return false;
    }
    if (undistImg.empty())
        return false;

    cv::warpPerspective(
        undistImg, dst,
        H_,
        cfg_.outSize(),
        cv::INTER_LINEAR,
        cv::BORDER_CONSTANT,
        cv::Scalar::all(0));

    return true;
}

bool CameraCorrector::save(const std::string& path) const
{
    cv::FileStorage fs(path, cv::FileStorage::WRITE);
    if (!fs.isOpened())
    {
        qDebug() << "[CameraCorrector] 无法打开: " << QString::fromStdString(path) << "\n";
        return false;
    }
    fs << "gridCols"     << cfg_.gridCols;
    fs << "gridRows"     << cfg_.gridRows;
    fs << "squareMm"     << cfg_.squareMm;
    fs << "outWidthMm"   << cfg_.outWidthMm;
    fs << "outHeightMm"  << cfg_.outHeightMm;
    fs << "pixPerMm"     << cfg_.pixPerMm;
    fs << "physicalOffsetXmm" << physicalOffsetMm_.x;
    fs << "physicalOffsetYmm" << physicalOffsetMm_.y;
    if (!H_.empty())
        fs << "homography" << H_;
    fs.release();
    qDebug() << "[CameraCorrector] 参数已保存: " << QString::fromStdString(path) << "\n";
    return true;
}

bool CameraCorrector::load(const std::string& path)
{
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) return false;
    fs["gridCols"]    >> cfg_.gridCols;
    fs["gridRows"]    >> cfg_.gridRows;
    fs["squareMm"]    >> cfg_.squareMm;
    fs["outWidthMm"]  >> cfg_.outWidthMm;
    fs["outHeightMm"] >> cfg_.outHeightMm;
    fs["pixPerMm"]    >> cfg_.pixPerMm;
    if (!fs["physicalOffsetXmm"].empty())
        fs["physicalOffsetXmm"] >> physicalOffsetMm_.x;
    else
        physicalOffsetMm_.x = 0.0;

    if (!fs["physicalOffsetYmm"].empty())
        fs["physicalOffsetYmm"] >> physicalOffsetMm_.y;
    else
        physicalOffsetMm_.y = 0.0;
    if (!fs["homography"].empty())
    {
        fs["homography"] >> H_;
        hasMap_ = !H_.empty();
    }
    fs.release();
    return hasMap_;
}

bool CameraCorrector::setCoordinateOriginByPair(cv::Point2d pixelPt,
                                                cv::Point2d physicalPtMm)
{
    if (cfg_.pixPerMm <= 0.0)
    {
        std::cerr << "[BoardHomography] pixPerMm 非法，无法设置二次初始化偏移。\n";
        return false;
    }

    const cv::Point2d baseMm(pixelPt.x / cfg_.pixPerMm,
                             pixelPt.y / cfg_.pixPerMm);
    physicalOffsetMm_ = physicalPtMm - baseMm;

    std::cout << "[BoardHomography] 二次初始化偏移(mm): ("
              << physicalOffsetMm_.x << ", "
              << physicalOffsetMm_.y << ")\n";
    return true;
}
