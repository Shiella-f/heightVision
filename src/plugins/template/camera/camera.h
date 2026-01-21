#pragma once
#include <QWidget>
#include <opencv2/opencv.hpp>
#include <QImage>
#include <QMessageBox>
#include <QObject>

class MyCamera : public QObject {
    Q_OBJECT
    
public:
    MyCamera();
    ~MyCamera();
    
    void usbCamera();
    void loadCalibrationData(); // 加载标定数据
    bool isCameraOpen() const { return cap.isOpened(); }
    void CameraRelease() {
        if (cap.isOpened()) {
            cap.release();
        }
    }


public slots:
    cv::Mat updateCameraFrame();
    
private:

    cv::VideoCapture cap;

    cv::Mat m_cameraMatrix;
    cv::Mat m_distCoeffs;
    cv::Mat m_map1, m_map2;
    bool m_isCameraCalibLoaded = false;
    cv::Size m_calibImageSize;

    QRect roi_3x3 = QRect(0, 0, 0, 0); // 3x3 ROI，默认中心区域

    cv::Mat HomographyMatrix; // 用于存储单应性矩阵
    bool m_is9_9CalibLoadedr = false;
    bool m_is3_3CalibLoaded = false;

};