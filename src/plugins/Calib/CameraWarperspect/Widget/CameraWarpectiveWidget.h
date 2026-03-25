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

// 相机变换界面类
class CameraWarpectiveWidget : public baseWidget
{
      Q_OBJECT
public:
    explicit CameraWarpectiveWidget(QWidget *parent = nullptr);
    ~CameraWarpectiveWidget();

private slots:
    bool onOpenImagesClicked();
    void onChangeShowClicked();
    void onCalibrateClicked();
    void onSaveClicked();
    void onTestClicked(); // 测试按钮槽函数
signals:
    void signalNextStep();

private:
    void init();

    QLineEdit* m_editBoardWidth;    // 输入棋盘格宽(角点数)的输入框
    QLineEdit* m_editBoardHeight;   // 输入棋盘格高(角点数)的输入框
    QLineEdit* m_editSquareSize;    // 输入方格尺寸(mm)的输入框
    QLineEdit* m_editOutWidth;    // 输出图片宽度输入框
    QLineEdit* m_editOutHeight;  // 输出图片高度输入框chu
    QLineEdit* m_editpixPerMm;  // 输出分辨率
    QTextEdit* m_textResult;        // 显示标定结果的文本框
    
    QPushButton* m_btnOpenImages;    // 打开图片按钮
    QPushButton* m_btnChangeShow;  // 切换显示按钮
    QPushButton* m_btnCalibrate;    // 标定按钮
    QPushButton* m_btnSave;         // 保存结果按钮
    bool isSaved = false;

    CameraCorrector *m_CameraCorrector; // 相机校正器实例
    CameraCorrector::Config m_config;

    WarpectiveScene* m_scene;
    QGraphicsView* m_view;

    std::vector<std::string> imagePaths;
    cv::Mat m_currentImage;
    cv::Mat m_rectifiedImage;
    bool isCurrentImageShow = true;
    bool isCalibrated = false; // 是否已标定成功

};