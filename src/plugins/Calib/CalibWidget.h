#pragma once
#include <QWidget>
#include "CameraCalib/Widget/CameraCalibWidget.h"
#include "Mirrorcalib/Widget/MirrorCalibWidget.h"
#include "CameraWarperspect/Widget/CameraWarpectiveWidget.h"
#include <QTabWidget>

    
class CalibWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CalibWidget(QWidget *parent = nullptr);
    ~CalibWidget();
private slots:
    //void onNextStep();

private:
    void init();
private:
    CameraCalibWidget* m_cameraCalibWidget;
    CameraWarpectiveWidget * m_cameraWarpectiveWidget;
    MirrorCalibWidget* m_mirrorCalibWidget;
    QTabWidget* m_tabWidget;
};