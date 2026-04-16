#include "CameraWarpectiveFunction.h"
#include <QFileDialog>
#include <QDebug>
#include <QApplication>
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include <QStandardPaths>
#include <QDir>
#include <QtGlobal>
#include "../../../../common/tools/bscvTool.h"

#include <QWidget>

using namespace std;
using namespace cv;


CameraWarpectiveFunction::CameraWarpectiveFunction(QWidget *parent)
    : m_scene(nullptr), m_CameraCorrector(nullptr)
{
    m_scene = new WarpectiveScene(nullptr);
    cv::Mat m = cv::Mat::zeros(3,3,CV_8UC1);
}

CameraWarpectiveFunction::~CameraWarpectiveFunction()
{
}

void CameraWarpectiveFunction::ChangeShow()
{
    if(isCurrentImageShow) {
        if (m_rectifiedImage.empty())return;
        QImage img;
        TIGER_BSVISION::cvImage2qImage(img, m_rectifiedImage);
        m_scene->setOriginalPixmap(QPixmap::fromImage(img));
        m_scene->resetImage();
        isCurrentImageShow = false;
    } else {
        if (m_currentImage.empty()) return;
        QImage img;
        TIGER_BSVISION::cvImage2qImage(img, m_currentImage);
        m_scene->setOriginalPixmap(QPixmap::fromImage(img));
        m_scene->resetImage();
        isCurrentImageShow = true;
    }
}

bool CameraWarpectiveFunction::CalibWarptive(const Config& config, const cv::Mat& inputImage)
{
    if(m_CameraCorrector == nullptr) {
        m_CameraCorrector = new CameraCorrector(config);
    } else {
        delete m_CameraCorrector;
        m_CameraCorrector = new CameraCorrector(config);
    }
    try {

        if(!m_CameraCorrector->computeMap(inputImage)){
            cerr << "Error: Failed to compute homography map" << endl;
            return false;
        }
        isCalibrated = true;
    } catch (const std::exception& e) {
        cerr << "Error during rectification: " << e.what() << endl;
       
    }

    return isCalibrated;
}

bool CameraWarpectiveFunction::SaveResult(const QString savePath)
{
    if (!m_CameraCorrector) return false;
    return m_CameraCorrector->save(savePath.toStdString());
}

bool CameraWarpectiveFunction::UseWarptiveImage(const cv::Mat& srcImage, cv::Mat& dstImage) const
{
    if (!isCalibrated) {
        return false;
    }
    return m_CameraCorrector->warp(srcImage, dstImage);
}

CameraWarptivePort* newCameraWarptivePort(){
    return new CameraWarpectiveFunction();
}


ICameraWarptiveSeene* CameraWarpectiveFunction::getScene() const {
    return m_scene;
}