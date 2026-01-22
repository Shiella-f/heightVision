#pragma once
#include <QWidget>
#include <QGraphicsView>
#include <QToolButton>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include "Mirrorcalib/Core/MirrorCalib.h"
#include "../Scene/MirrorScene.h"
#include "../common/Widget/baseWidget.h"

class MirrorCalibWidget : public baseWidget {
    Q_OBJECT

public:
    explicit MirrorCalibWidget(QWidget *parent = nullptr);
    ~MirrorCalibWidget();

private slots:
    void onSnap3x3Image();         
    void onSnap9x9Image();         
    void onCalibrate3x3();         
    void onCalibrate9x9();         
    void onConfirmSize();
    void onSaveCalibData();       


private:
    void init();
    void logMessage(const QString& msg);
    cv::Mat getCurrentCameraImage();

    MirrorCalib* m_core;
    MirrorScene* m_MirrorScene;
    QGraphicsView* m_view;

    QToolButton* m_Snap3x3btn;
    QToolButton* m_Snap9x9btn;
    QToolButton* m_Calib3x3btn;
    QToolButton* m_Calib9x9btn;
    QToolButton* m_ConfirmSizebtn;
    QToolButton* m_savedatebtn;
    QToolButton* m_clearRoibtn;

    QCheckBox* m_setRoibox;
    
    QTableWidget* m_resultTable; 

    cv::Mat m_currentImage;
    
    std::vector<cv::Point2f> m_3x3Points;
    std::vector<cv::Point2f> m_9x9Points;

    QRectF m_roiArea = QRectF();  // ROI区域
};
