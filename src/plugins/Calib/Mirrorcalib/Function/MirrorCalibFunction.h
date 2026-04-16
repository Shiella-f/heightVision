#pragma once
#include <QWidget>
#include "Mirrorcalib/Core/MirrorCalib.h"
#include <QTableWidget>
#include "../Scene/MirrorScene.h"
#include "../../CameraWarperspect/Core/WArpective.h"
#include "MirrorCalibPort.h"

class MirrorCalibFunction : public QObject, public CalibWarptivePort {
    Q_OBJECT

public:
    explicit MirrorCalibFunction(QWidget *parent = nullptr);
    ~MirrorCalibFunction();

    bool Calibrate3x3(const cv::Mat& image) override;//3x3标定
    bool Calibrate9x9(const cv::Mat& image) override;//9x9标定
    bool Save3x3Result(const QString savePath) override;// 保存3x3标定结果
    QVector<QPointF> Get9x9Points() const override; // 获取9x9点坐标
    IMirrorScene* getScene() const override;//返回场景指针

private slots:              
    void onConfirmSize();     

private:
    cv::Mat getCurrentCameraImage();

    MirrorCalib* m_core = nullptr;
    MirrorScene* m_MirrorScene = nullptr;
    QTableWidget* m_resultTable = nullptr;
    cv::Mat m_currentImage;
    
    std::vector<cv::Point2f> m_3x3Points;
    std::vector<cv::Point2f> m_9x9Points;

    QRectF m_roiArea = QRectF();  // ROI区域
    CameraCorrector *corrector = nullptr;
    cv::Point2f m_Canvassize = cv::Point2f(110.0f, 110.0f); // 当前坐标点

    void updatePoint(const QPointF& point);
};
