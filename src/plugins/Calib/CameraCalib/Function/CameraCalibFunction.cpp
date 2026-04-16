#include "CameraCalibFunction.h"
#include <QDebug>
#include <QApplication>
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <QDebug>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

CameraCalibFunction::CameraCalibFunction(QWidget *parent)
    : m_scene(nullptr)
{
    m_scene = new CameraCalibScene();
    cv::Mat m = cv::Mat::zeros(3,3,CV_8UC1);
}

CameraCalibFunction::~CameraCalibFunction()
{
}

bool CameraCalibFunction::Calibrate(QVector<QString> imagePaths, cv::Size boardSize, float squareSize,
                                        bool useCharuco, const float markerSize, const int dictionaryId)
{
    cv::Mat cameraMatrix, distCoeffs;
    std::vector<cv::Mat> rvecs, tvecs;
    std::vector<std::vector<cv::Point2f>> imagePoints;
    std::vector<std::vector<cv::Point3f>> objectPoints;
    std::vector<std::string> imagePathsStr;

    for (const auto& path : imagePaths) {
        imagePathsStr.push_back(path.toLocal8Bit().constData());
    }


    // 判断是否使用 ChArUco
    double rms = -1.0;
    if (useCharuco) {
        rms = m_calib.runCalibrationChArUco(imagePathsStr, boardSize, squareSize, markerSize, dictionaryId, 
                                            cameraMatrix, distCoeffs, rvecs, tvecs, imagePoints, objectPoints);
    } else {
        rms = m_calib.runCalibration(imagePathsStr, boardSize, squareSize, cameraMatrix, distCoeffs, rvecs, tvecs, imagePoints, objectPoints);
    }

    if (rms >= 0) {
        // 保存结果到成员变量
        m_isCalibrated = true;
        m_result.cameraMatrix = cameraMatrix;
        m_result.distCoeffs = distCoeffs;
        m_result.rvecs = rvecs;
        m_result.tvecs = tvecs;
        m_result.rms = rms;
        imagePoints = imagePoints;
        objectPoints = objectPoints;

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
    if (!m_isCalibrated) return false;
    if (savePath.isEmpty()) return false;
    // 根据后缀名判断保存格式
    if (savePath.endsWith(".xml") || savePath.endsWith(".yaml") || savePath.endsWith(".yml")) {
        try {
            cv::FileStorage fs(savePath.toStdString(), cv::FileStorage::WRITE);
            
            // 1. 标定时间
            fs << "calibration_time" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
            
            // 2. 图像尺寸
            fs << "image_width" << m_imageSize.width;
            fs << "image_height" << m_imageSize.height;
            
            // 3. 相机内参矩阵
            fs << "camera_matrix" << m_result.cameraMatrix;
            
            // 4. 畸变系数
            fs << "distortion_coefficients" << m_result.distCoeffs;
            
            // 5. 重投影误差
            fs << "rms_error" << m_result.rms;
            
            // 6. 外参数 (每张图片的旋转和平移)
            fs << "rvecs" << m_result.rvecs;
            fs << "tvecs" << m_result.tvecs;

            // 7. 可选：计算并保存优化后的新相机矩阵 (New Camera Matrix)
            // alpha=1 保留所有像素，alpha=0 裁剪掉无效像素
            cv::Mat newCameraMatrix = cv::getOptimalNewCameraMatrix(m_result.cameraMatrix, m_result.distCoeffs, m_imageSize, 1, m_imageSize, 0);
            fs << "new_camera_matrix" << newCameraMatrix;

            fs.release();
            isSaved = true;
        } catch (const cv::Exception& e) {
            isSaved = false;
        }
    }
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