#include "CameraCalibFunction.h"
#include <QDebug>
#include <QApplication>
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <cmath>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include "../../BoardConfiguration.h"

using namespace std;
using namespace cv;

CameraCalibFunction::CameraCalibFunction(QWidget *parent)
    : m_scene(nullptr)
{
    m_scene = new CameraCalibScene();
    m_calib.setLogCallback([this](const QString& message){emit calibrationLog(message);});
    cv::Mat m = cv::Mat::zeros(3,3,CV_8UC1);
}

CameraCalibFunction::~CameraCalibFunction()
{
}

bool CameraCalibFunction::CalibrateBoard(const QVector<QString>& paths,const QJsonObject& board){
    QJsonObject normalized;QString error;
    if(!calibration::NormalizeBoardConfiguration(board,normalized,error)){
        m_isCalibrated=false;m_result={};m_imageSize={};m_pendingBoard={};m_imagePoints.clear();m_objectPoints.clear();emit calibrationLog(error);return false;
    }
    m_pendingBoard=normalized;
    return Calibrate(paths,{normalized["squares_x"].toInt(),normalized["squares_y"].toInt()},float(normalized["square_length"].toDouble()));
}
bool CameraCalibFunction::Calibrate(QVector<QString> imagePaths, cv::Size boardSize, float squareSize,
                                        bool useCharuco, const float markerSize, const int dictionaryId)
{
    m_isCalibrated = false;
    isSaved = false;
    m_imageSize = {};
    m_result = {};
    m_imagePoints.clear();
    m_objectPoints.clear();
    cv::Mat cameraMatrix, distCoeffs;
    std::vector<cv::Mat> rvecs, tvecs;
    std::vector<std::vector<cv::Point2f>> imagePoints;
    std::vector<std::vector<cv::Point3f>> objectPoints;
    std::vector<std::string> imagePathsStr;

    for (const auto& path : imagePaths) {
        imagePathsStr.push_back(path.toUtf8().toStdString());
    }


    // 判断是否使用 ChArUco
    double rms = -1.0;
    const auto configured=m_pendingBoard;m_pendingBoard={};
    if(!configured.isEmpty()){
        rms=m_calib.runConfigured(imagePathsStr,configured,cameraMatrix,distCoeffs,rvecs,tvecs,imagePoints,objectPoints);
    }else if (useCharuco) {
        rms = m_calib.runCalibrationChArUco(imagePathsStr, boardSize, squareSize, markerSize, dictionaryId, 
                                            cameraMatrix, distCoeffs, rvecs, tvecs, imagePoints, objectPoints);
    } else {
        rms = m_calib.runCalibration(imagePathsStr, boardSize, squareSize, cameraMatrix, distCoeffs, rvecs, tvecs, imagePoints, objectPoints);
    }

    if (std::isfinite(rms) && rms >= 0 && m_calib.imageSize().area() > 0) {
        m_imageSize = m_calib.imageSize();
        // 保存结果到成员变量
        m_isCalibrated = true;
        m_result.cameraMatrix = cameraMatrix;
        m_result.distCoeffs = distCoeffs;
        m_result.rvecs = rvecs;
        m_result.tvecs = tvecs;
        m_result.rms = rms;
        m_imagePoints = imagePoints;
        m_objectPoints = objectPoints;

        QString resultStr;
        resultStr += QString("标定成功！\n");
        resultStr += QString("重投影误差 (RMS Error): %1\n\n").arg(rms);
        
        resultStr += "相机内参矩阵 (Camera Matrix):\n";
        // 格式化输出内参矩阵
        resultStr += QString("[%1, %2, %3]\n").arg(m_result.cameraMatrix.at<double>(0,0)).arg(m_result.cameraMatrix.at<double>(0,1)).arg(m_result.cameraMatrix.at<double>(0,2));
        resultStr += QString("[%1, %2, %3]\n").arg(m_result.cameraMatrix.at<double>(1,0)).arg(m_result.cameraMatrix.at<double>(1,1)).arg(m_result.cameraMatrix.at<double>(1,2));
        resultStr += QString("[%1, %2, %3]\n\n").arg(m_result.cameraMatrix.at<double>(2,0)).arg(m_result.cameraMatrix.at<double>(2,1)).arg(m_result.cameraMatrix.at<double>(2,2));

        resultStr += "畸变系数 (Distortion Coefficients):\n";
        // 畸变系数(k1, k2, p1, p2, k3)
        for (int i = 0; i < m_result.distCoeffs.cols; ++i) {
             resultStr += QString("%1 ").arg(m_result.distCoeffs.at<double>(0, i));
        }
        resultStr += "\n";

        return true;
    } else {
        m_isCalibrated = false;
        return false;
    }
}

bool CameraCalibFunction::SaveResult(const QString savePath)
{
    isSaved = false;
    if (!m_isCalibrated || m_imageSize.width <= 0 || m_imageSize.height <= 0 || savePath.isEmpty()) return false;
    const bool xml = savePath.endsWith(".xml", Qt::CaseInsensitive);
    if (!xml && !savePath.endsWith(".yml", Qt::CaseInsensitive) && !savePath.endsWith(".yaml", Qt::CaseInsensitive)) return false;
    try {
        cv::FileStorage fs("", cv::FileStorage::WRITE | cv::FileStorage::MEMORY
                           | (xml ? cv::FileStorage::FORMAT_XML : cv::FileStorage::FORMAT_YAML));
        fs << "calibration_time" << QDateTime::currentDateTime().toString(Qt::ISODate).toStdString()
           << "image_width" << m_imageSize.width << "image_height" << m_imageSize.height
           << "camera_matrix" << m_result.cameraMatrix << "distortion_coefficients" << m_result.distCoeffs
           << "rms_error" << m_result.rms << "rvecs" << m_result.rvecs << "tvecs" << m_result.tvecs;
        const cv::Mat newCameraMatrix = cv::getOptimalNewCameraMatrix(m_result.cameraMatrix,
            m_result.distCoeffs, m_imageSize, 1, m_imageSize);
        fs << "new_camera_matrix" << newCameraMatrix;
        fs << "calibration_scope" << "camera_numeric_solution"
           << "physical_validation_status" << "unverified"
           << "solver" << "multistart_200_iterations";
        const auto contents = fs.releaseAndGetString();
        QSaveFile file(savePath);
        isSaved = file.open(QIODevice::WriteOnly)
            && file.write(contents.data(), static_cast<qint64>(contents.size())) == static_cast<qint64>(contents.size())
            && file.commit();
    } catch (const cv::Exception&) { isSaved = false; }
    return isSaved;
}

CameraCalibPort* newCameraCalibPort(){
    return new CameraCalibFunction();
}

ICameraCalibScene* CameraCalibFunction::getScene() const {
    return m_scene;
}

CameraCalibPort::CalibData CameraCalibFunction::getCalibData() const {
    return m_result;
}

bool CameraCalibFunction::isCalibrated() const {
    return m_isCalibrated;
}
