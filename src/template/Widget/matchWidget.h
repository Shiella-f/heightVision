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
    void displayRoiImage(const QRectF &roi);


private:
    void init();
    void setRoi();
    void learnMatch();
    void confirmMatch();
    void confirmRoiMatch();
    void confirmRoiarea();
    void TestHeight();

private:
    QToolButton* m_setRoiBtn;
    QToolButton* m_learnMatchBtn;
    QToolButton* m_confirmMatchBtn;
    QToolButton* m_confirmRoiMatchBtn;
    QToolButton* m_confirmRoiareaBtn;

    QWidget* m_imageDisplayWidget;
    ZoomScene* m_ImageDisplayScene;
    QWidget* m_roiDisplayWidget;
    ImageDisplayScene* m_RoiDisplayScene;

    cv::Mat m_currentImage;

    QRectF m_Roi;






    QToolButton* m_testHeightBtn;
    HeightMainWindow* m_heightMainWindow;
   
};