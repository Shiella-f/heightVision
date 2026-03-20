
#include "Warpective.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

// ============================================================
//  Stage 1: 畸变校正
// ============================================================

void CameraCorrector::setIntrinsics(const cv::Mat& cameraMatrix,
                                    const cv::Mat& distCoeffs,
                                    const cv::Size& imageSize)
{
    intrinsic_.cameraMatrix = cameraMatrix.clone();
    intrinsic_.distCoeffs   = distCoeffs.clone();
    intrinsic_.imageSize    = imageSize;
    intrinsicsReady_ = true;
    mapsReady_       = false;   // 内参变了，旧映射表失效
}

// ------------------------------------------------------------

void CameraCorrector::loadIntrinsics(const std::string& filePath)
{
    cv::FileStorage fs(filePath, cv::FileStorage::READ);
    if (!fs.isOpened())
        throw std::runtime_error("CameraCorrector: cannot open " + filePath);

    fs["camera_matrix "] >> intrinsic_.cameraMatrix;
    fs["distortion_coefficients"]   >> intrinsic_.distCoeffs;

    int w = 0, h = 0;
    fs["image_width"]  >> w;
    fs["image_height"] >> h;
    intrinsic_.imageSize = cv::Size(w, h);

    intrinsicsReady_ = true;
    mapsReady_       = false;
    std::cout << "[CameraCorrector] Intrinsics loaded from " << filePath << "\n";
}

// ------------------------------------------------------------

void CameraCorrector::saveIntrinsics(const std::string& filePath) const
{
    if (!intrinsicsReady_)
        throw std::runtime_error("CameraCorrector: no intrinsics to save");

    cv::FileStorage fs(filePath, cv::FileStorage::WRITE);
    fs << "cameraMatrix" << intrinsic_.cameraMatrix;
    fs << "distCoeffs"   << intrinsic_.distCoeffs;
    fs << "imageWidth"   << intrinsic_.imageSize.width;
    fs << "imageHeight"  << intrinsic_.imageSize.height;
    std::cout << "[CameraCorrector] Intrinsics saved to " << filePath << "\n";
}

// ------------------------------------------------------------

double CameraCorrector::calibrateFromImages(const std::vector<cv::Mat>& images,
                                             const cv::Size& boardSize,
                                             float squareSize)
{
    std::vector<std::vector<cv::Point3f>> objectPoints;
    std::vector<std::vector<cv::Point2f>> imagePoints;

    // 生成棋盘格3D坐标（Z=0平面）
    std::vector<cv::Point3f> objPts;
    for (int r = 0; r < boardSize.height; ++r)
        for (int c = 0; c < boardSize.width; ++c)
            objPts.emplace_back(c * squareSize, r * squareSize, 0.f);

    cv::Size detectedSize;
    int validCount = 0;

    for (const auto& img : images) {
        cv::Mat gray;
        if (img.channels() == 3)
            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        else
            gray = img;

        std::vector<cv::Point2f> corners;
        bool found = cv::findChessboardCorners(
            gray, boardSize, corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);

        if (!found) continue;

        // 亚像素精化
        cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
            cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.001));

        objectPoints.push_back(objPts);
        imagePoints.push_back(corners);
        detectedSize = gray.size();
        ++validCount;
    }

    if (validCount < 3)
        throw std::runtime_error("CameraCorrector: need at least 3 valid calibration images");

    std::vector<cv::Mat> rvecs, tvecs;
    double rms = cv::calibrateCamera(
        objectPoints, imagePoints, detectedSize,
        intrinsic_.cameraMatrix, intrinsic_.distCoeffs,
        rvecs, tvecs);

    intrinsic_.imageSize = detectedSize;
    intrinsicsReady_ = true;
    mapsReady_       = false;

    std::cout << "[CameraCorrector] Calibration RMS = " << rms
              << "  (valid images: " << validCount << "/" << images.size() << ")\n";
    return rms;
}

// ------------------------------------------------------------

void CameraCorrector::initUndistortMaps()
{
    if (!intrinsicsReady_)
        throw std::runtime_error("CameraCorrector: set intrinsics before initUndistortMaps");

    // getOptimalNewCameraMatrix 保留尽量多的有效像素（alpha=1），
    // 也可设 alpha=0 以裁掉黑边
    cv::Mat newK = cv::getOptimalNewCameraMatrix(
        intrinsic_.cameraMatrix, intrinsic_.distCoeffs,
        intrinsic_.imageSize, /*alpha=*/1.0, intrinsic_.imageSize);

    cv::initUndistortRectifyMap(
        intrinsic_.cameraMatrix, intrinsic_.distCoeffs,
        cv::Mat(), newK,
        intrinsic_.imageSize, CV_16SC2,
        map1_, map2_);

    mapsReady_ = true;
}

// ------------------------------------------------------------

void CameraCorrector::correctDistortion(const cv::Mat& src,
                                         cv::Mat& dst,
                                         bool crop)
{
    if (!intrinsicsReady_)
        throw std::runtime_error("CameraCorrector: intrinsics not set");

    if (mapsReady_) {
        cv::remap(src, dst, map1_, map2_, cv::INTER_LINEAR);
    } else {
        cv::undistort(src, dst,
                      intrinsic_.cameraMatrix,
                      intrinsic_.distCoeffs);
    }

    if (crop) {
        // 计算有效像素区域并裁剪（去掉畸变矫正后的黑边）
        cv::Mat newK = cv::getOptimalNewCameraMatrix(
            intrinsic_.cameraMatrix, intrinsic_.distCoeffs,
            intrinsic_.imageSize, 0.0);   // alpha=0 → 最大内接矩形

        cv::Rect roi;
        cv::getOptimalNewCameraMatrix(
            intrinsic_.cameraMatrix, intrinsic_.distCoeffs,
            intrinsic_.imageSize, 0.0, intrinsic_.imageSize, &roi);

        if (roi.area() > 0)
            dst = dst(roi).clone();
    }
}


// ============================================================
//  Stage 2: 倾斜校正
// ============================================================

void CameraCorrector::setTiltCorrectionByPoints(
    const std::vector<cv::Point2f>& srcPoints,
    const std::vector<cv::Point2f>& dstPoints,
    const cv::Size& outputSize)
{
    if (srcPoints.size() != 4 || dstPoints.size() != 4)
        throw std::runtime_error("CameraCorrector: need exactly 4 point pairs");

    tilt_.homography  = cv::getPerspectiveTransform(srcPoints, dstPoints);
    tilt_.outputSize  = outputSize;
    tiltReady_ = true;

    std::cout << "[CameraCorrector] Tilt homography set from 4-point pairs\n";
}

// ------------------------------------------------------------

bool CameraCorrector::autoDetectTilt(const cv::Mat& referenceImage,
                                      const cv::Size& outputSize,
                                      cv::Mat* debugView)
{
    cv::Mat gray;
    if (referenceImage.channels() == 3)
        cv::cvtColor(referenceImage, gray, cv::COLOR_BGR2GRAY);
    else
        gray = referenceImage.clone();

    std::vector<cv::Point2f> corners;
    if (!findLargestQuad(gray, corners, debugView)) {
        std::cerr << "[CameraCorrector] autoDetectTilt: no quad found\n";
        return false;
    }

    // 对检测到的4个角点按顺序排列
    corners = sortCorners(corners);

    // 计算目标矩形尺寸（用透视变换后矩形的宽高）
    auto dist2d = [](const cv::Point2f& a, const cv::Point2f& b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    };
    float w = std::max(dist2d(corners[0], corners[1]),
                       dist2d(corners[3], corners[2]));
    float h = std::max(dist2d(corners[0], corners[3]),
                       dist2d(corners[1], corners[2]));

    cv::Size outSz = (outputSize.area() > 0) ? outputSize
                                             : cv::Size((int)w, (int)h);

    std::vector<cv::Point2f> dstPts = {
        { 0.f,          0.f          },
        { (float)outSz.width - 1, 0.f },
        { (float)outSz.width - 1, (float)outSz.height - 1 },
        { 0.f,          (float)outSz.height - 1 }
    };

    tilt_.homography = cv::getPerspectiveTransform(corners, dstPts);
    tilt_.outputSize = outSz;
    tiltReady_ = true;

    std::cout << "[CameraCorrector] autoDetectTilt: quad found, "
              << "output size = " << outSz << "\n";
    return true;
}

// ------------------------------------------------------------

void CameraCorrector::estimateTiltFromLines(const cv::Mat& referenceImage,
                                             bool isHorizontal)
{
    cv::Mat gray;
    if (referenceImage.channels() == 3)
        cv::cvtColor(referenceImage, gray, cv::COLOR_BGR2GRAY);
    else
        gray = referenceImage.clone();

    double angle = estimateDominantAngle(gray, isHorizontal);
    std::cout << "[CameraCorrector] Estimated tilt angle = " << angle << " deg\n";

    cv::Size outSz;
    tilt_.homography = angleToHomography(angle, gray.size(), outSz);
    tilt_.outputSize = outSz;
    tiltReady_ = true;
}

// ------------------------------------------------------------

void CameraCorrector::setHomography(const cv::Mat& H, const cv::Size& outputSize)
{
    tilt_.homography = H.clone();
    tilt_.outputSize = outputSize;
    tiltReady_ = true;
}

// ------------------------------------------------------------

void CameraCorrector::saveTiltParams(const std::string& filePath) const
{
    if (!tiltReady_)
        throw std::runtime_error("CameraCorrector: no tilt params to save");

    cv::FileStorage fs(filePath, cv::FileStorage::WRITE);
    fs << "homography"    << tilt_.homography;
    fs << "outputWidth"   << tilt_.outputSize.width;
    fs << "outputHeight"  << tilt_.outputSize.height;
    std::cout << "[CameraCorrector] Tilt params saved to " << filePath << "\n";
}

// ------------------------------------------------------------

void CameraCorrector::loadTiltParams(const std::string& filePath)
{
    cv::FileStorage fs(filePath, cv::FileStorage::READ);
    if (!fs.isOpened())
        throw std::runtime_error("CameraCorrector: cannot open " + filePath);

    fs["homography"] >> tilt_.homography;
    int w = 0, h = 0;
    fs["outputWidth"]  >> w;
    fs["outputHeight"] >> h;
    tilt_.outputSize = cv::Size(w, h);
    tiltReady_ = true;
    std::cout << "[CameraCorrector] Tilt params loaded from " << filePath << "\n";
}

// ------------------------------------------------------------

void CameraCorrector::correctTilt(const cv::Mat& src, cv::Mat& dst)
{
    if (!tiltReady_)
        throw std::runtime_error("CameraCorrector: tilt params not set");

    cv::warpPerspective(src, dst, tilt_.homography,
                        tilt_.outputSize,
                        cv::INTER_LINEAR,
                        cv::BORDER_CONSTANT, cv::Scalar(0));
}


// ============================================================
//  全流程一键接口
// ============================================================

void CameraCorrector::correct(const cv::Mat& src,
                               cv::Mat& dst,
                               CorrectionInfo* info)
{
    cv::Mat tmp = src;

    CorrectionInfo localInfo;

    if (intrinsicsReady_) {
        correctDistortion(tmp, dst);
        tmp = dst;
        localInfo.distortionApplied = true;
    }

    if (tiltReady_) {
        correctTilt(tmp, dst);
        localInfo.tiltApplied = true;
    } else if (!intrinsicsReady_) {
        // 两阶段都未准备，直接透传
        dst = src.clone();
    }

    if (info) *info = localInfo;
}


// ============================================================
//  私有工具方法
// ============================================================

std::vector<cv::Point2f> CameraCorrector::sortCorners(
    const std::vector<cv::Point2f>& pts)
{
    // 按 y 排序，取最上面两点和最下面两点
    std::vector<cv::Point2f> sorted = pts;
    std::sort(sorted.begin(), sorted.end(),
              [](const cv::Point2f& a, const cv::Point2f& b){ return a.y < b.y; });

    // 上两点按 x 排：左上、右上
    if (sorted[0].x > sorted[1].x) std::swap(sorted[0], sorted[1]);
    // 下两点按 x 排：左下、右下（注意顺序：左上→右上→右下→左下）
    if (sorted[2].x > sorted[3].x) std::swap(sorted[2], sorted[3]);

    // 重排为 左上、右上、右下、左下
    return { sorted[0], sorted[1], sorted[3], sorted[2] };
}

// ------------------------------------------------------------

bool CameraCorrector::findLargestQuad(const cv::Mat& gray,
                                       std::vector<cv::Point2f>& corners,
                                       cv::Mat* debugView)
{
    cv::Mat blurred, edges;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
    cv::Canny(blurred, edges, 50, 150);

    // 膨胀以连接断开的边缘
    cv::dilate(edges, edges, cv::Mat(), cv::Point(-1, -1), 2);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double maxArea = 0;
    std::vector<cv::Point> bestQuad;

    for (auto& cnt : contours) {
        double area = cv::contourArea(cnt);
        if (area < 1000) continue;  // 过滤噪声

        std::vector<cv::Point> approx;
        double peri = cv::arcLength(cnt, true);
        cv::approxPolyDP(cnt, approx, 0.02 * peri, true);

        if (approx.size() == 4 && cv::isContourConvex(approx) && area > maxArea) {
            maxArea = area;
            bestQuad = approx;
        }
    }

    if (bestQuad.empty()) return false;

    corners.clear();
    for (auto& p : bestQuad)
        corners.emplace_back((float)p.x, (float)p.y);

    if (debugView) {
        if (debugView->empty() || debugView->size() != gray.size())
            cv::cvtColor(gray, *debugView, cv::COLOR_GRAY2BGR);
        std::vector<std::vector<cv::Point>> quads = { bestQuad };
        cv::drawContours(*debugView, quads, 0, cv::Scalar(0, 255, 0), 2);
        for (auto& p : corners)
            cv::circle(*debugView, p, 6, cv::Scalar(0, 0, 255), -1);
    }

    return true;
}

// ------------------------------------------------------------

double CameraCorrector::estimateDominantAngle(const cv::Mat& gray,
                                               bool isHorizontal)
{
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    std::vector<cv::Vec2f> lines;
    cv::HoughLines(edges, lines, 1, CV_PI / 180.0, 100);

    if (lines.empty()) {
        std::cerr << "[CameraCorrector] estimateDominantAngle: no lines found\n";
        return 0.0;
    }

    // 收集角度
    std::vector<double> angles;
    for (auto& l : lines) {
        double theta = l[1] * 180.0 / CV_PI;  // 0~180 度
        if (isHorizontal) {
            // 水平线：theta 接近 90°
            if (std::abs(theta - 90.0) < 30.0)
                angles.push_back(theta - 90.0);
        } else {
            // 垂直线：theta 接近 0° 或 180°
            if (theta < 30.0 || theta > 150.0)
                angles.push_back(theta < 90.0 ? theta : theta - 180.0);
        }
    }

    if (angles.empty()) return 0.0;

    // 取中值，减少离群值影响
    std::sort(angles.begin(), angles.end());
    return angles[angles.size() / 2];
}

// ------------------------------------------------------------

cv::Mat CameraCorrector::angleToHomography(double angleDeg,
                                            const cv::Size& imageSize,
                                            cv::Size& outputSize)
{
    // 以图像中心为旋转中心，生成旋转矩阵
    cv::Point2f center(imageSize.width / 2.f, imageSize.height / 2.f);
    cv::Mat rot = cv::getRotationMatrix2D(center, angleDeg, 1.0);

    // 计算旋转后的外接矩形大小（保留完整内容）
    double cosA = std::abs(std::cos(angleDeg * CV_PI / 180.0));
    double sinA = std::abs(std::sin(angleDeg * CV_PI / 180.0));
    int newW = (int)(imageSize.height * sinA + imageSize.width  * cosA);
    int newH = (int)(imageSize.height * cosA + imageSize.width  * sinA);

    // 调整平移，使旋转后的图像居中
    rot.at<double>(0, 2) += (newW / 2.0 - center.x);
    rot.at<double>(1, 2) += (newH / 2.0 - center.y);

    outputSize = cv::Size(newW, newH);

    // 将 2×3 仿射矩阵扩充为 3×3 单应矩阵
    cv::Mat H = cv::Mat::eye(3, 3, CV_64F);
    rot.copyTo(H(cv::Rect(0, 0, 3, 2)));
    return H;
}