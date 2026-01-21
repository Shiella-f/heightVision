#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QGraphicsView>
#include "../Core/Calib.h"
#include "../../common/Widget/baseWidget.h"
#include "../Scene/CameraCalibScene.h"

// 相机标定界面类
class CameraCalibWidget : public baseWidget
{
    Q_OBJECT
public:
    explicit CameraCalibWidget(QWidget *parent = nullptr);
    ~CameraCalibWidget();

private slots:
    void onAddImagesClicked();
    void onRemoveImageClicked();
    void onCalibrateClicked();
    void onSaveClicked();
signals:
    void signalNextStep();

private:
    void init();

    QListWidget* m_listImages;      // 显示图片路径的列表控件
    QLineEdit* m_editBoardWidth;    // 输入棋盘格宽(角点数)的输入框
    QLineEdit* m_editBoardHeight;   // 输入棋盘格高(角点数)的输入框
    QLineEdit* m_editBoardDepth;    // 输入棋盘厚度输入框
    QLineEdit* m_editSquareSize;    // 输入方格尺寸(mm)的输入框
    QLineEdit* m_editMarkerSize;    // 输入ArUco Marker尺寸(mm)
    QLineEdit* m_editDictionaryId;  // 输入ArUco字典ID(为空则使用普通标定)
    QTextEdit* m_textResult;        // 显示标定结果的文本框
    
    QPushButton* m_btnAddImages;    // 添加图片按钮
    QPushButton* m_btnRemoveImage;  // 移除图片按钮
    QPushButton* m_btnCalibrate;    // 标定按钮
    QPushButton* m_btnSave;         // 保存结果按钮
    bool isSaved = false;
    Calib m_calib;
    CameraCalibScene* m_scene;
    QGraphicsView* m_view;

    // 标定结果数据
    bool m_isCalibrated = false;
    cv::Mat m_cameraMatrix;
    cv::Mat m_distCoeffs;
    std::vector<cv::Mat> m_rvecs;
    std::vector<cv::Mat> m_tvecs;
    double m_rms = 0.0;
    cv::Size m_imageSize;
    std::vector<std::vector<cv::Point2f>> m_imagePoints;
    std::vector<std::vector<cv::Point3f>> m_objectPoints;

    std::vector<std::string> imagePaths;
    cv::Mat m_currentImage;
};