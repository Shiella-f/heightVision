#include "MeasurementCalibration.h"
#include <QJsonDocument>
#include "PerspectiveFitReport.h"
#include "PlaneModel.h"
#include <QFile>
#include <QSaveFile>
#include <opencv2/calib3d.hpp>
#include <cmath>

namespace {
bool finiteMatrix(const cv::Mat& matrix)
{
    return !matrix.empty() && matrix.channels() == 1 && matrix.depth() == CV_64F
        && cv::checkRange(matrix);
}

bool projectionMatrix(const cv::Mat& matrix)
{
    return finiteMatrix(matrix) && matrix.size() == cv::Size(3, 3)
        && matrix.at<double>(0, 0) > 0 && matrix.at<double>(1, 1) > 0
        && std::abs(matrix.at<double>(2, 0)) < 1e-12
        && std::abs(matrix.at<double>(2, 1)) < 1e-12
        && std::abs(matrix.at<double>(2, 2) - 1.0) < 1e-12
        && std::abs(cv::determinant(matrix)) > 1e-12;
}

bool readMatrix(const cv::FileStorage& fs, const char* name, cv::Mat& matrix)
{
    if (fs[name].empty()) return false;
    cv::Mat input;
    fs[name] >> input;
    if (input.empty() || input.channels() != 1) return false;
    input.convertTo(matrix, CV_64F);
    return finiteMatrix(matrix);
}

bool readNumber(const cv::FileStorage& fs, const char* name, double& number)
{
    const auto node = fs[name];
    if (!node.isInt() && !node.isReal()) return false;
    number = static_cast<double>(node);
    return std::isfinite(number);
}

bool readCamera(const cv::FileStorage& fs, MeasurementCalibration& result)
{
    const auto width = fs["image_width"], height = fs["image_height"];
    if (!width.isInt() || !height.isInt()) return false;
    result.imageSize = {static_cast<int>(width), static_cast<int>(height)};
    return readMatrix(fs, "camera_matrix", result.cameraMatrix)
        && readMatrix(fs, "distortion_coefficients", result.distCoeffs)
        && readMatrix(fs, "new_camera_matrix", result.newCameraMatrix)
        && result.hasCamera();
}

cv::FileStorage readStorage(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto bytes = file.readAll();
    if (bytes.isEmpty()) return {};
    return cv::FileStorage(bytes.toStdString(), cv::FileStorage::READ | cv::FileStorage::MEMORY);
}
}

bool MeasurementCalibration::hasCamera() const
{
    if(plane)return false;
    const size_t count = distCoeffs.total();
    return imageSize.width > 0 && imageSize.height > 0
        && projectionMatrix(cameraMatrix) && projectionMatrix(newCameraMatrix)
        && finiteMatrix(distCoeffs) && (distCoeffs.rows == 1 || distCoeffs.cols == 1)
        && (count == 4 || count == 5 || count == 8 || count == 12 || count == 14);
}

bool MeasurementCalibration::ready() const
{
    if(plane)return plane->valid()&&plane->regionValid;
    if (!hasCamera() || !finiteMatrix(homography) || homography.size() != cv::Size(3, 3)
        || !std::isfinite(pixPerMm) || pixPerMm <= 0
        || !std::isfinite(offsetMm.x) || !std::isfinite(offsetMm.y)) return false;
    const double scale = cv::norm(homography, cv::NORM_INF);
    if (!(scale > 0)) return false;
    cv::Mat inverse;
    return cv::invert(homography / scale, inverse, cv::DECOMP_SVD) > 1e-12;
}

bool MeasurementCalibration::loadCamera(const QString& path)
{
    *this = {};
    try {
        const auto fs = readStorage(path);
        MeasurementCalibration candidate;
        if(fs.isOpened()&&!fs["model_type"].empty())return false;
        if (!fs.isOpened() || !readCamera(fs, candidate)) return false;
        *this = std::move(candidate);
        return true;
    } catch (const cv::Exception&) { return false; }
}

bool MeasurementCalibration::loadProfile(const QString& path)
{
    *this = {};
    try {
        const auto fs = readStorage(path);
        if(fs.isOpened()&&fs["format_version"].isInt()&&int(fs["format_version"])==2){
            auto model=std::make_shared<calibration::PlaneModel>();QString error;
            if(!calibration::PlaneModel::load(path,*model,true,error))return false;
            imageSize=model->imageSize;plane=std::move(model);return true;
        }
        if (!fs.isOpened() || !fs["format_version"].isInt()
            || static_cast<int>(fs["format_version"]) != 1
            || static_cast<std::string>(fs["input_stage"]) != "raw"
            || static_cast<std::string>(fs["homography_input"]) != "undistorted_pixels"
            || static_cast<std::string>(fs["unit"]) != "mm") return false;
        MeasurementCalibration candidate;
        if (!readCamera(fs, candidate)
            || !readMatrix(fs, "homography", candidate.homography)
            || !readNumber(fs, "pixPerMm", candidate.pixPerMm)
            || !readNumber(fs, "physicalOffsetXmm", candidate.offsetMm.x)
            || !readNumber(fs, "physicalOffsetYmm", candidate.offsetMm.y)
            || !candidate.ready()) return false;
        // Normalize H so the projective-denominator check is scale independent.
        candidate.homography /= cv::norm(candidate.homography, cv::NORM_INF);
        *this = std::move(candidate);
        return true;
    } catch (const cv::Exception&) { return false; }
}

bool MeasurementCalibration::saveProfile(const QString& path) const
{
    return saveProfile(path, nullptr);
}

bool MeasurementCalibration::saveProfile(const QString& path, const calibration::PerspectiveFitReport* report) const
{
    if(plane)return plane->save(path,true);
    if (!ready()) return false;
    try {
        cv::FileStorage fs("", cv::FileStorage::WRITE | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_YAML);
        fs << "format_version" << 1 << "input_stage" << "raw"
           << "homography_input" << "undistorted_pixels" << "unit" << "mm"
           << "image_width" << imageSize.width << "image_height" << imageSize.height
           << "camera_matrix" << cameraMatrix << "distortion_coefficients" << distCoeffs
           << "new_camera_matrix" << newCameraMatrix << "homography" << homography
           << "pixPerMm" << pixPerMm << "physicalOffsetXmm" << offsetMm.x
           << "physicalOffsetYmm" << offsetMm.y;
        if (report) report->write(fs);
        if(!regionMetadata.isEmpty()){
            const auto bytes=QJsonDocument(regionMetadata).toJson(QJsonDocument::Compact).toBase64();
            fs<<"region_metadata_encoding"<<"base64_chunks"<<"region_metadata_json"<<"[";
            for(qsizetype at=0;at<bytes.size();at+=512)fs<<bytes.mid(at,512).toStdString();
            fs<<"]";
        }
        const auto contents = fs.releaseAndGetString();
        QSaveFile file(path);
        return file.open(QIODevice::WriteOnly)
            && file.write(contents.data(), static_cast<qint64>(contents.size())) == static_cast<qint64>(contents.size())
            && file.commit();
    } catch (const cv::Exception&) { return false; }
}

bool MeasurementCalibration::undistortImage(const cv::Mat& raw, cv::Mat& output) const
{
    output.release();
    if (!hasCamera() || raw.empty() || raw.dims != 2 || raw.size() != imageSize
        || raw.depth() != CV_8U || (raw.channels() != 1 && raw.channels() != 3 && raw.channels() != 4)) return false;
    cv::undistort(raw, output, cameraMatrix, distCoeffs, newCameraMatrix);
    return !output.empty();
}

bool MeasurementCalibration::mapPoints(const std::vector<cv::Point2f>& raw,
                                      std::vector<cv::Point2d>& millimeters) const
{
    millimeters.clear();
    if(plane)return plane->mapPoints(raw,millimeters);
    if (!ready() || raw.empty()) return false;
    std::vector<cv::Point2d> source(raw.begin(), raw.end()), undistorted, rectified;
    for (const auto& point : source)
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
    cv::undistortPoints(source, undistorted, cameraMatrix, distCoeffs, cv::noArray(), newCameraMatrix,
                        cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 100, 1e-13));
    const cv::Mat normalized = homography / cv::norm(homography, cv::NORM_INF);
    for (const auto& point : undistorted) {
        const double denominator = normalized.at<double>(2, 0) * point.x
            + normalized.at<double>(2, 1) * point.y + normalized.at<double>(2, 2);
        if (!std::isfinite(point.x) || !std::isfinite(point.y)
            || !std::isfinite(denominator) || std::abs(denominator) < 1e-12) return false;
    }
    cv::perspectiveTransform(undistorted, rectified, normalized);
    for (auto& point : rectified) {
        point = point * (1.0 / pixPerMm) + offsetMm;
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
    }
    millimeters = std::move(rectified);
    return true;
}
