#pragma once
#include <QWidget>
#include <opencv2/opencv.hpp>
#include <QImage>
#include <QMessageBox>
#include <QObject>
#include "../template_global.h"
#include "../../../interfaces/Imageprocess.h" // 包含接口定义

class ImageProcess : public QObject, public IImageProcess {
    Q_OBJECT
    
public:
    ImageProcess();
    
    // 实现接口虚函数
    void loadCalibrationData() override; // 加载标定数据
    cv::Mat CalibImage(cv::Mat &originalImage) override; // 注意：也是槽函数
    bool isCalibrationLoaded() const { return m_isCameraCalibLoaded || m_is3_3CalibLoaded; }

public slots:
   
    
private:
    cv::Mat m_cameraMatrix;
    cv::Mat m_distCoeffs;
    cv::Mat m_map1, m_map2;
    bool m_isCameraCalibLoaded = false;
    cv::Size m_calibImageSize;
    QRect roi_3x3 = QRect(0, 0, 0, 0); // 3x3 ROI，默认中心区域

    cv::Mat HomographyMatrix; // 用于存储单应性矩阵
    bool m_is9_9CalibLoaded = false;
    bool m_is3_3CalibLoaded = false;

};

// 导出创建实例的工厂函数
extern "C" TEMPLATE_EXPORT IImageProcess* CreateImageProcessInstance();