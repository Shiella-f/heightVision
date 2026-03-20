#pragma once
#include <opencv2/opencv.hpp>
#include <QWidget>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QLineEdit>
#include <QCheckBox>
#include <QSlider>

// struct CameraParamInfo {
//     QString name;        // 参数名称
//     int id;             // OpenCV参数ID
//     bool isSupported;   // 是否支持
//     double minVal;      // 最小值
//     double maxVal;      // 最大值
//     double currentVal;  // 当前值
//     double stepVal;     // 步长（调整的最小单位）
// };

// // 获取指定相机（index）指定属性（propId）的范围信息
// // 如果获取成功返回 true，并将结果填入 info.minVal, info.maxVal, info.stepVal, info.isSupported
// bool getCameraParamRange(int cameraIndex, int propId, CameraParamInfo& info);

class CameraPara : public QWidget {
    Q_OBJECT
public:
    explicit CameraPara(QWidget* parent = nullptr);
    ~CameraPara();
    // bool OpenCamera(); // Removed or deprecated
    // bool isOpened() const; // Removed or deprecated
    // cv::Mat getCurrentFrame(); // Removed or deprecated

    struct Camerapara
    {
        bool autoExposureSupported;
        int exposureValue;
        int gainValue;
        int brightnessValue;
        int contrastValue;
        int saturationValue;
        int toneValue;
        int gammaValue;
        int fpsValue;
        bool autoFocusSupported;
        int focusValue;
    };
    
    Camerapara getParameters() const { return m_camerapara; }

signals:
    void parametersChanged(const CameraPara::Camerapara& params);

private:
    void init();
    void updateParameters(); // Helper to emit signal
    Camerapara m_camerapara;
    // cv::VideoCapture cap; // Removed ownership

    QCheckBox* m_autoExposureCheckBox;
    QSlider* m_exposureSlider;
    QSlider* m_gainSlider;
    QSlider* m_brightnessSlider;
    QSlider* m_contrastSlider;
    QSlider* m_saturationSlider;
    QSlider* m_toneSlider;
    QSlider* m_gammaSlider;
    QSlider* m_fpsSlider;
    QCheckBox* m_autoFocusCheckBox;
    QSlider* m_focusSlider;


};