#include "PerspectiveRemap.h"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <cmath>

namespace calibration {
namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
bool same(const cv::Mat& a, const cv::Mat& b) {
    return a.type() == b.type() && a.size() == b.size() && cv::norm(a, b, cv::NORM_INF) == 0;
}
bool supported(cv::Size size) {
    // OpenCV remap uses signed-short coordinates internally for image dimensions.
    return size.width > 0 && size.height > 0 && size.width < 32767 && size.height < 32767;
}
}

bool buildPerspectiveMaps(const MeasurementCalibration& calibration, cv::Size outputSize,
                          cv::Mat& map1, cv::Mat& map2, int mapType) {
    map1.release();
    map2.release();
    if (!calibration.ready() || !supported(calibration.imageSize) || !supported(outputSize)
        || (mapType != CV_16SC2 && mapType != CV_32FC2)) return false;
    try {
        // q -> inverse(H) q -> inverse(K_new) -> camera distortion -> raw pixel.
        // OpenCV 4.7 inverts the full new-camera matrix here, including its third row.
        const cv::Mat projection = calibration.homography * calibration.newCameraMatrix;
        cv::initUndistortRectifyMap(calibration.cameraMatrix, calibration.distCoeffs,
            cv::Mat(), projection, outputSize, mapType, map1, map2);

        // Preserve the finite undistorted-image domain of the former two-pass path.
        const cv::Mat inverse = calibration.homography.inv();
        const double* a = inverse.ptr<double>();
        const double tolerance = 1e-12 * (std::abs(a[6]) * outputSize.width
            + std::abs(a[7]) * outputSize.height + std::abs(a[8]));
        cv::parallel_for_(cv::Range(0, outputSize.height), [&](const cv::Range& rows) {
            for (int y = rows.start; y < rows.end; ++y) {
                for (int x = 0; x < outputSize.width; ++x) {
                    double denominator = a[6]*x + a[7]*y + a[8];
                    double u = (a[0]*x + a[1]*y + a[2]) / denominator;
                    double v = (a[3]*x + a[4]*y + a[5]) / denominator;
                    bool valid = std::abs(denominator) > tolerance && std::isfinite(u) && std::isfinite(v)
                        && u >= 0 && v >= 0 && u <= calibration.imageSize.width-1 && v <= calibration.imageSize.height-1;
                    if (mapType == CV_16SC2) {
                        if (!valid) { map1.at<cv::Vec2s>(y,x) = {-32768,-32768}; map2.at<ushort>(y,x) = 0; }
                    } else {
                        auto& point = map1.at<cv::Vec2f>(y,x);
                        if (!valid || !std::isfinite(point[0]) || !std::isfinite(point[1])) point = {-1000000.f,-1000000.f};
                    }
                }
            }
        });
        return true;
    } catch (const cv::Exception&) {
        map1.release(); map2.release(); return false;
    }
}

struct PerspectiveRemapper::Mapping {
    MeasurementCalibration calibration;
    cv::Size outputSize;
    cv::Mat map1, map2;
    bool matches(const MeasurementCalibration& other, cv::Size output) const {
        return outputSize == output && calibration.imageSize == other.imageSize
            && same(calibration.cameraMatrix, other.cameraMatrix) && same(calibration.distCoeffs, other.distCoeffs)
            && same(calibration.newCameraMatrix, other.newCameraMatrix) && same(calibration.homography, other.homography);
    }
};

void PerspectiveRemapper::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mapping.reset();
}

bool PerspectiveRemapper::render(const cv::Mat& raw, const MeasurementCalibration& calibration,
                                cv::Size outputSize, cv::Mat& output, RemapTiming& timing) const {
    // Keep a source header before clearing output, so UseWarptiveImage(image,image) works.
    const cv::Mat input = raw;
    output.release(); timing = {};
    if (!calibration.ready() || input.empty() || input.dims != 2 || input.size() != calibration.imageSize
        || input.depth() != CV_8U || (input.channels()!=1 && input.channels()!=3 && input.channels()!=4)) {
        timing.error = "invalid calibration or raw image"; return false;
    }
    try {
        std::shared_ptr<const Mapping> mapping;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            timing.reused = m_mapping && m_mapping->matches(calibration, outputSize);
            if (!timing.reused) {
                auto start = Clock::now();
                auto candidate = std::make_shared<Mapping>();
                candidate->calibration = calibration;
                candidate->calibration.cameraMatrix = calibration.cameraMatrix.clone();
                candidate->calibration.distCoeffs = calibration.distCoeffs.clone();
                candidate->calibration.newCameraMatrix = calibration.newCameraMatrix.clone();
                candidate->calibration.homography = calibration.homography.clone();
                candidate->outputSize = outputSize;
                if (!buildPerspectiveMaps(candidate->calibration, outputSize, candidate->map1, candidate->map2)) {
                    timing.error = "cannot build perspective map"; return false;
                }
                m_mapping = std::move(candidate);
                timing.buildMs = elapsed(start);
            }
            mapping = m_mapping;
        }
        auto start = Clock::now();
        cv::Mat sampled;
        cv::remap(input, sampled, mapping->map1, mapping->map2, cv::INTER_LINEAR,
                  cv::BORDER_CONSTANT, cv::Scalar::all(0));
        timing.sampleMs = elapsed(start);
        output = std::move(sampled);
        return true;
    } catch (const cv::Exception& error) {
        timing.error = error.what(); output.release(); return false;
    }
}
}
