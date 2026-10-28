#include "MirrorCalib.h"
#include <QLocale>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace {
struct Marker {
    cv::Point2f center;
    float radius;
    double quality;
};

float median(std::vector<float> values)
{
    const auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}

std::vector<Marker> findCandidates(const cv::Mat& binary)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    std::vector<Marker> markers;
    for (const auto& contour : contours) {
        const cv::Rect bounds = cv::boundingRect(contour);
        if (bounds.width < 4 || bounds.height < 4)
            continue;
        std::vector<cv::Point> hull;
        cv::convexHull(contour, hull);
        const double area = cv::contourArea(hull);
        const double perimeter = cv::arcLength(hull, true);
        if (area < 8 || area > binary.total() * 0.15 || perimeter <= 0)
            continue;
        const double circularity = 4 * CV_PI * area / (perimeter * perimeter);
        if (circularity < 0.80 || cv::contourArea(contour) < area * 0.5)
            continue;
        cv::Point2f center;
        float radius;
        cv::minEnclosingCircle(hull, center, radius);
        if (hull.size() >= 5) {
            const auto ellipse = cv::fitEllipse(hull);
            const float minor = std::min(ellipse.size.width, ellipse.size.height);
            const float major = std::max(ellipse.size.width, ellipse.size.height);
            if (minor < major * 0.6f)
                continue;
            center = ellipse.center;
            radius = (minor + major) * 0.25f;
        }
        bool duplicate = false;
        for (auto& previous : markers) {
            if (cv::norm(center - previous.center) < 0.6f * std::min(radius, previous.radius)) {
                if (circularity > previous.quality)
                    previous = {center, radius, circularity};
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            markers.push_back({center, radius, circularity});
    }
    return markers;
}

bool compatible(const Marker& a, const Marker& b)
{
    return std::min(a.radius, b.radius) >= std::max(a.radius, b.radius) * 0.6f;
}

// Discover the lattice independently of the requested N. A sliding window
// inside a larger grid would silently accept the wrong plate.
bool orderGrid(const std::vector<cv::Point2f>& points, std::vector<cv::Point2f>& ordered)
{
    const int side = static_cast<int>(std::lround(std::sqrt(points.size())));
    if (side < 3 || side * side != static_cast<int>(points.size()))
        return false;
    float angle = cv::minAreaRect(points).angle;
    while (angle >= 45.0f) angle -= 90.0f;
    while (angle < -45.0f) angle += 90.0f;
    const float radians = angle * static_cast<float>(CV_PI / 180.0);
    const cv::Point2f horizontal(std::cos(radians), std::sin(radians));
    const cv::Point2f vertical(-horizontal.y, horizontal.x);
    struct Projected { cv::Point2f source; float x; float y; };
    std::vector<Projected> projected;
    for (const auto& point : points)
        projected.push_back({point, point.dot(horizontal), point.dot(vertical)});
    std::sort(projected.begin(), projected.end(), [](const auto& a, const auto& b) { return a.y < b.y; });
    std::vector<float> rowMeans, rowGaps, columnGaps;
    for (int row = 0; row < side; ++row) {
        auto start = projected.begin() + row * side;
        std::sort(start, start + side, [](const auto& a, const auto& b) { return a.x < b.x; });
        float mean = 0;
        for (int col = 0; col < side; ++col) {
            mean += start[col].y;
            if (col > 0)
                columnGaps.push_back(start[col].x - start[col - 1].x);
        }
        rowMeans.push_back(mean / side);
        if (row > 0)
            rowGaps.push_back(rowMeans[row] - rowMeans[row - 1]);
    }
    const float rowSpacing = median(rowGaps);
    const float columnSpacing = median(columnGaps);
    if (rowSpacing <= 0 || columnSpacing <= 0)
        return false;
    auto consistent = [](const std::vector<float>& gaps, float spacing) {
        return std::all_of(gaps.begin(), gaps.end(), [spacing](float gap) {
            return gap > spacing * 0.55f && gap < spacing * 1.65f;
        });
    };
    if (!consistent(rowGaps, rowSpacing) || !consistent(columnGaps, columnSpacing))
        return false;
    for (int row = 0; row < side; ++row) {
        const auto start = projected.begin() + row * side;
        for (int col = 0; col < side; ++col) {
            if (std::abs(start[col].y - rowMeans[row]) > rowSpacing * 0.30f)
                return false;
            // Also validate columns: equal counts alone cannot accept a missing
            // marker replaced by an unrelated circular object.
            if (row > 0 && std::abs(start[col].x - projected[col].x) > columnSpacing * 0.35f)
                return false;
        }
    }
    ordered.clear();
    for (const auto& point : projected)
        ordered.push_back(point.source);
    return true;
}

std::vector<std::vector<cv::Point2f>> findGrids(const std::vector<Marker>& markers)
{
    const size_t count = markers.size();
    if (count < 9 || count > 15000)
        return {};
    std::vector<float> nearest(count, std::numeric_limits<float>::max());
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            if (!compatible(markers[i], markers[j]))
                continue;
            const cv::Point2f delta = markers[i].center - markers[j].center;
            const float distance = std::sqrt(delta.dot(delta));
            const float radius = std::max(markers[i].radius, markers[j].radius);
            if (distance < radius * 1.8f)
                continue;
            nearest[i] = std::min(nearest[i], distance);
            nearest[j] = std::min(nearest[j], distance);
        }
    }
    std::vector<size_t> parent(count);
    std::iota(parent.begin(), parent.end(), 0);
    auto root = [&parent](size_t index) {
        while (parent[index] != index) {
            parent[index] = parent[parent[index]];
            index = parent[index];
        }
        return index;
    };
    for (size_t i = 0; i < count; ++i) {
        if (nearest[i] == std::numeric_limits<float>::max())
            continue;
        for (size_t j = i + 1; j < count; ++j) {
            if (!compatible(markers[i], markers[j]))
                continue;
            const cv::Point2f delta = markers[i].center - markers[j].center;
            const float limit = std::min(nearest[i], nearest[j]) * 1.30f;
            if (delta.dot(delta) <= limit * limit)
                parent[root(j)] = root(i);
        }
    }
    std::vector<std::vector<cv::Point2f>> components(count), grids;
    for (size_t i = 0; i < count; ++i)
        components[root(i)].push_back(markers[i].center);
    for (const auto& component : components) {
        std::vector<cv::Point2f> ordered;
        if (orderGrid(component, ordered))
            grids.push_back(std::move(ordered));
    }
    return grids;
}

bool sameGrid(const std::vector<cv::Point2f>& a, const std::vector<cv::Point2f>& b)
{
    if (a.size() != b.size())
        return false;
    const float tolerance = static_cast<float>(cv::norm(a[1] - a[0]) * 0.20);
    for (size_t i = 0; i < a.size(); ++i)
        if (cv::norm(a[i] - b[i]) > tolerance)
            return false;
    return true;
}
}

bool MirrorCalib::isValidGridSize(int gridSize)
{
    return gridSize >= 3 && gridSize <= 65 && gridSize % 2 == 1;
}

bool MirrorCalib::findMarkerPoints(const cv::Mat& image, int gridSize,
                                  std::vector<cv::Point2f>& centers) const
{
    centers.clear();
    if (!isValidGridSize(gridSize) || image.empty() || image.dims != 2 || image.depth() != CV_8U)
        return false;
    cv::Mat gray;
    switch (image.channels()) {
    case 1: gray = image; break;
    case 3: cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY); break;
    case 4: cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY); break;
    default: return false;
    }
    if (std::min(gray.rows, gray.cols) < 9)
        return false;
    cv::Mat global, local;
    cv::threshold(gray, global, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    const int blockSize = std::min(51, (std::min(gray.rows, gray.cols) / 2) * 2 - 1);
    cv::adaptiveThreshold(gray, local, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, blockSize, 5);
    std::vector<std::vector<cv::Point2f>> hypotheses;
    for (const cv::Mat& binary : {global, local}) {
        for (int polarity = 0; polarity < 2; ++polarity) {
            cv::Mat mask;
            if (polarity == 0) mask = binary;
            else cv::bitwise_not(binary, mask);
            for (auto& grid : findGrids(findCandidates(mask))) {
                if (std::none_of(hypotheses.begin(), hypotheses.end(), [&grid](const auto& other) { return sameGrid(grid, other); }))
                    hypotheses.push_back(std::move(grid));
            }
        }
    }
    // Multiple complete grids (including competing interpretations) are ambiguous.
    if (hypotheses.size() != 1 || hypotheses.front().size() != static_cast<size_t>(gridSize * gridSize))
        return false;
    centers = std::move(hypotheses.front());
    return true;
}

bool MirrorCalib::savePointsToCSV(const QString& filepath,
                                 const std::vector<cv::Point2f>& points,
                                 const std::vector<cv::Point2d>& physicalPoints) const
{
    if (filepath.isEmpty() || points.empty() || points.size() != physicalPoints.size())
        return false;
    QSaveFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    stream.setLocale(QLocale::c());
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream.setRealNumberPrecision(6);
    stream << "Index,PixelX,PixelY,PhysicalXmm,PhysicalYmm\n";
    for (size_t i = 0; i < points.size(); ++i)
        stream << static_cast<qulonglong>(i) << ',' << points[i].x << ',' << points[i].y
               << ',' << physicalPoints[i].x << ',' << physicalPoints[i].y << '\n';
    stream.flush();
    return stream.status() == QTextStream::Ok && file.commit();
}
