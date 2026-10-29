#include "MirrorCalibFunction.h"
#include "tools/bscvTool.h"
#include <QFile>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include "../../PlaneModel.h"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

MirrorCalibFunction::MirrorCalibFunction(QWidget* parent)
    : QObject(parent), m_scene(new MirrorScene(this))
{
}

void MirrorCalibFunction::clearResult()
{
    m_points.clear();
    m_physicalPoints.clear();
    m_scene->clearOverlays();
}

bool MirrorCalibFunction::LoadCalibrationData(const QString& path)
{
    clearResult();
    clearCalibration();
    m_scene->setOriPixmap(QPixmap());
    if (!m_calibration.loadBaseProfile(path)) return false;
    if (m_calibration.plane) {
        const auto& plane = *m_calibration.plane;
        m_outputSize = plane.outputSize;
        m_pixelToMm = calibration::halfPixelToMm(plane.outputSize, plane.regionSize);
    } else {
        m_outputSize = m_calibration.outputPixels();
        m_pixelToMm = m_calibration.pixelToMm();
        m_renderCalibration = m_calibration;
        m_renderCalibration.homography = m_calibration.renderHomography();
    }
    if (m_outputSize.empty() || m_pixelToMm.empty() || !cv::checkRange(m_pixelToMm)) {
        clearCalibration();
        return false;
    }
    return true;
}

void MirrorCalibFunction::clearCalibration()
{
    m_calibration = {};
    m_renderCalibration = {};
    m_outputSize = {};
    m_pixelToMm.release();
    m_planeMapX.release();
    m_planeMapY.release();
    m_remapper.reset();
}

bool MirrorCalibFunction::renderBase(const cv::Mat& raw, cv::Mat& base)
{
    base.release();
    if (m_calibration.plane) {
        if ((m_planeMapX.empty() || m_planeMapY.empty())
            && !m_calibration.plane->buildMaps(m_planeMapX, m_planeMapY)) return false;
        cv::remap(raw, base, m_planeMapX, m_planeMapY, cv::INTER_LINEAR, cv::BORDER_CONSTANT);
        return !base.empty();
    }
    calibration::RemapTiming timing;
    return m_remapper.render(raw, m_renderCalibration, m_outputSize, base, timing);
}

bool MirrorCalibFunction::rectifiedPointsToMm(const std::vector<cv::Point2f>& points,
                                             std::vector<cv::Point2d>& millimeters) const
{
    millimeters.clear();
    if (points.empty() || m_pixelToMm.empty()) return false;
    std::vector<cv::Point2d> pixels(points.begin(), points.end()), result;
    for (auto p : pixels) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x < 0 || p.y < 0
            || p.x > m_outputSize.width-1 || p.y > m_outputSize.height-1) return false;
    }
    cv::perspectiveTransform(pixels, result, m_pixelToMm);
    for (auto p : result) if (!std::isfinite(p.x) || !std::isfinite(p.y)) return false;
    if (m_calibration.plane) {
        const auto& plane = *m_calibration.plane;
        const cv::Point2d origin = plane.regionCenter
            - cv::Point2d(plane.regionSize.width/2, plane.regionSize.height/2);
        for (auto p : result) {
            cv::Point2d raw;
            if (!plane.worldToRaw(origin+p, raw)
                || cv::pointPolygonTest(plane.supportHull, raw, true) < -1) return false;
        }
    } else {
        // Detection pixels are already rectified. Only inverse-project to verify source
        // support; never feed them to mapPoints(), which expects raw camera pixels.
        std::vector<cv::Point2d> undistorted, normalized, raw;
        cv::perspectiveTransform(pixels, undistorted, m_renderCalibration.homography.inv());
        for (auto p : undistorted) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x < 0 || p.y < 0
                || p.x > m_calibration.imageSize.width-1 || p.y > m_calibration.imageSize.height-1) return false;
        }
        cv::perspectiveTransform(undistorted, normalized, m_calibration.newCameraMatrix.inv());
        std::vector<cv::Point3d> rays;
        for (auto p : normalized) rays.emplace_back(p.x, p.y, 1);
        cv::projectPoints(rays, cv::Vec3d(), cv::Vec3d(), m_calibration.cameraMatrix,
                          m_calibration.distCoeffs, raw);
        for (auto p : raw) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x < 0 || p.y < 0
                || p.x > m_calibration.imageSize.width-1 || p.y > m_calibration.imageSize.height-1) return false;
        }
    }
    millimeters = std::move(result);
    return true;
}

bool MirrorCalibFunction::Calibrate(const cv::Mat& image, int gridSize)
{
    clearResult();
    m_scene->setOriPixmap(QPixmap());
    try {
        if (!m_calibration.ready() || !MirrorCalib::isValidGridSize(gridSize)
            || image.empty() || image.dims != 2 || image.size() != m_calibration.imageSize
            || image.depth() != CV_8U || (image.channels()!=1 && image.channels()!=3 && image.channels()!=4))
            return false;
        cv::Mat base;
        if (!renderBase(image, base)) return false;
        QImage display;
        if (!TIGER_BSVISION::cvImage2qImage(display, base)) return false;
        m_scene->setOriPixmap(QPixmap::fromImage(display));
        std::vector<cv::Point2f> points;
        std::vector<cv::Point2d> physical;
        if (!m_core.findMarkerPoints(base, gridSize, points)
            || !rectifiedPointsToMm(points, physical)) return false;
        m_points = std::move(points);
        m_physicalPoints = std::move(physical);
        const double radius = std::clamp(std::min(base.cols, base.rows) / (gridSize * 20.0), 1.0, 5.0);
        for (const auto& point : m_points)
            // OpenCV pixel (0,0) is the first pixel center; Qt pixmaps place it at (.5,.5).
            m_scene->addOverlayPoint(QPointF(point.x+.5, point.y+.5), QPen(Qt::red), radius);
        return true;
    } catch (const cv::Exception& error) {
        qWarning() << "Mirror calibration:" << error.what();
        clearResult();
        return false;
    }
}

bool MirrorCalibFunction::Calibrate(const QString& imagePath, int gridSize)
{
    clearResult();
    m_scene->setOriPixmap(QPixmap());
    if (!m_calibration.ready() || !MirrorCalib::isValidGridSize(gridSize))
        return false;
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty())
        return false;
    try {
        const cv::Mat encoded(1, static_cast<int>(bytes.size()), CV_8UC1, const_cast<char*>(bytes.constData()));
        return Calibrate(cv::imdecode(encoded, cv::IMREAD_COLOR), gridSize);
    } catch (const cv::Exception& error) {
        qWarning() << "Mirror image decode:" << error.what();
        return false;
    }
}

QVector<QPointF> MirrorCalibFunction::GetPoints() const
{
    QVector<QPointF> result;
    result.reserve(static_cast<qsizetype>(m_points.size()));
    for (const auto& point : m_points)
        result.append(QPointF(point.x, point.y));
    return result;
}

bool MirrorCalibFunction::SaveResult(const QString& savePath) const
{
    return m_core.savePointsToCSV(savePath, m_points, m_physicalPoints);
}

QVector<QPointF> MirrorCalibFunction::GetPhysicalPoints() const
{
    QVector<QPointF> result;
    result.reserve(static_cast<qsizetype>(m_physicalPoints.size()));
    for (const auto& point : m_physicalPoints)
        result.append(QPointF(point.x, point.y));
    return result;
}

IMirrorScene* MirrorCalibFunction::getScene() const
{
    return m_scene;
}

CalibWarptivePort* newCalibWarptivePort()
{
    return new MirrorCalibFunction();
}
