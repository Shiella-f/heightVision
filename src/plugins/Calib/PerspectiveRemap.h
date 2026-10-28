#pragma once
#include "MeasurementCalibration.h"
#include <memory>
#include <mutex>
#include <string>

namespace calibration {
// Internal pixel remapping only; the exported H and physical mapping are unchanged.
bool buildPerspectiveMaps(const MeasurementCalibration& calibration, cv::Size outputSize,
                          cv::Mat& map1, cv::Mat& map2, int mapType = CV_16SC2);

struct RemapTiming {
    bool reused = false;
    double buildMs = 0;
    double sampleMs = 0;
    std::string error;
};

class PerspectiveRemapper {
public:
    void reset();
    bool render(const cv::Mat& raw, const MeasurementCalibration& calibration,
                cv::Size outputSize, cv::Mat& output, RemapTiming& timing) const;
private:
    struct Mapping;
    mutable std::mutex m_mutex;
    mutable std::shared_ptr<const Mapping> m_mapping;
};
}
