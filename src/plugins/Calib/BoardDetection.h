#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>

// Internal implementation shared by camera and perspective calibration.
namespace calibration {
enum class BoardType { Chessboard, Charuco };
struct BoardSpec {
    BoardType type = BoardType::Chessboard;
    cv::Size squares;
    double squareMm = 1.0;
    double markerMm = 0.0;
    int dictionaryId = 0;
    std::vector<int> markerIds;
    std::vector<cv::Point3f> measuredCorners;
    int minimumMarkers = 0;
    int minimumCharucoCorners = 4;
    int interpolateMinMarkers = 2;
    cv::Size innerSize() const { return {squares.width - 1, squares.height - 1}; }
    bool valid() const;
};
struct DetectionAttempt {
    std::string method;
    cv::Size workSize;
    double milliseconds = 0;
    bool found = false;
};
struct BoardObservation {
    std::vector<cv::Point2f> pixels;
    std::vector<cv::Point3f> objectPoints; // mm, board outer grid origin
    std::vector<int> ids;                 // row-major inner-grid IDs
    std::vector<DetectionAttempt> attempts;
    int markerCount = 0;
    int rejectedGuides = 0;
    double milliseconds = 0;
    double refinementMs = 0;
    std::string method, error;
};
bool detectBoard(const cv::Mat& image, const BoardSpec& spec, BoardObservation& result);
}
