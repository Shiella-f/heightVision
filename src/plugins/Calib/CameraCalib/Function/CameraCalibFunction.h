#pragma once
#include <QWidget>
#include <QString>
#include <QVector>
#include "../Core/Calib.h"
#include "../Scene/CameraCalibScene.h"
#include "CameraCalibPort.h"

// 相机标定界面类
class CameraCalibFunction : public QObject, public CameraCalibPort
{
    Q_OBJECT
public:
    explicit CameraCalibFunction(QWidget *parent = nullptr);
    ~CameraCalibFunction();
    bool Calibrate(QVector<QString> imagePaths, cv::Size boardSize, float squareSize, 
                            bool useCharuco = false, const float markerSize = 0.0f,
                            const int dictionaryId = -1) override;
    bool SaveResult(const QString savePath) override;// 保存标定结果,后缀使用.XML或者.YAML
    ICameraCalibScene* getScene() const override;//返回场景指针
    CalibData getCalibData() const override; // 获取标定结果数据
    bool isCalibrated() const override; // 是否已完成标定


private:

    bool isSaved = false;
    Calib m_calib;
    CameraCalibScene* m_scene;

    
    // 标定结果数据
    CameraCalibPort::CalibData m_result;
    bool m_isCalibrated = false;
    cv::Size m_imageSize;
    std::vector<std::vector<cv::Point2f>> m_imagePoints;
    std::vector<std::vector<cv::Point3f>> m_objectPoints;

    QVector<QString> imagePaths;
    cv::Mat m_currentImage;
};