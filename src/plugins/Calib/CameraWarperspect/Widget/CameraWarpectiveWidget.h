#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QGraphicsView>
#include <QPointF>
#include <QRegExpValidator>
#include "../Core/Warpective.h"
#include "common/Widget/baseWidget.h"
#include "../Scene/CameraWarpectiveScene.h"

// 相机标定界面类
class CameraWarpectiveWidget : public baseWidget
{
    Q_OBJECT
public:
    explicit CameraWarpectiveWidget(QWidget *parent = nullptr);
    ~CameraWarpectiveWidget();

private slots:
    void onOpenImagesClicked();
    void onSelectPointsClicked();
    void SelectPoints(QPointF point);
    void onEnsurePointClicked();
    void onStartCalibClicked();
    void onSwitchShowClicked();
    void onSaveClicked();


signals:

private:
    void init();
    void connectAll();

    QLineEdit* m_ImgPointLd[4];   // 图片坐标显示标签
    QLineEdit* m_ObjPointLd[4];   // 实际坐标显示标签
    QLabel* LactionLabel[4];    
    QTextEdit* m_textResult;        // 显示标定结果的文本框
    
    QPushButton* m_OpenImages;    // 图片按钮
    QPushButton* m_selectPointsBtn;    // 选择点按钮
    QPushButton* m_EnsurePoint;    // 确定坐标
    QPushButton* m_StartBtn;         // 开始标定
    QPushButton* m_SwitchSHowBtn;         // 切换显示按钮
    QPushButton* m_SaveBtn;         // 保存结果按钮
    bool isSaved = false;
    CameraCorrector m_CameraCorrector;
    WarpectiveScene* m_scene;
    QGraphicsView* m_view;

    // 标定结果数据
    bool m_isCalibrated = false;
    QPointF m_ImgPoints[4]; // 4个角点坐标
    QPointF m_ObjPoints[4]; // 4个对象点坐标
    double m_rms = 0.0;
    cv::Size m_imageSize;
    cv::Mat m_currentImage;
    cv::Mat m_resultImage;
    bool m_showOriginal = true; // 是否显示原图
};