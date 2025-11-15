#pragma once
#include <QWidget>
#include "tools/bscvTool.h"
#include "Widget/HeightMainWindow.h"

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QToolButton;
class ImageDisplayScene;
class ZoomScene;
class matchWidget : public QWidget {
    Q_OBJECT

public:
    matchWidget(QWidget* parent = nullptr);
    ~matchWidget();

    void displayImage(const cv::Mat &image);


private:
    void init();
    void setRoi();
    void learnMatch();
    void confirmMatch();
    void confirmRoiMatch();
    void TestHeight();

private:
    QToolButton* m_setRoiBtn;
    QToolButton* m_learnMatchBtn;
    QToolButton* m_confirmMatchBtn;
    QToolButton* m_confirmRoiMatchBtn;

    QWidget* m_imageDisplayWidget;
    ZoomScene* m_ImageDisplayScene;
    QWidget* m_roiDisplayWidget;
    ImageDisplayScene* m_RoiDisplayScene;

    cv::Mat m_currentImage;




    QToolButton* m_testHeightBtn;
    HeightMainWindow* m_heightMainWindow;
   
};