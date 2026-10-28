#include "MirrorCalibFunction.h"
#include "tools/bscvTool.h"
#include <QFile>
#include <QDebug>
#include <algorithm>

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
    return m_calibration.loadProfile(path);
}

bool MirrorCalibFunction::Calibrate(const cv::Mat& image, int gridSize)
{
    clearResult();
    try {
        if (!m_calibration.ready() || image.empty() || image.dims != 2 || image.size() != m_calibration.imageSize)
            return false;
        if (!m_core.findMarkerPoints(image, gridSize, m_points))
            return false;
        if (!m_calibration.mapPoints(m_points, m_physicalPoints)) {
            clearResult();
            return false;
        }
        QImage display;
        if (!TIGER_BSVISION::cvImage2qImage(display, image)) {
            clearResult();
            return false;
        }
        m_scene->setOriPixmap(QPixmap::fromImage(display));
        const double radius = std::clamp(std::min(image.cols, image.rows) / (gridSize * 20.0), 1.0, 5.0);
        for (const auto& point : m_points)
            m_scene->addOverlayPoint(QPointF(point.x, point.y), QPen(Qt::red), radius);
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
