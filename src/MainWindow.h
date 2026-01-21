#pragma once
#include <QWidget>
#include <opencv2/opencv.hpp>
#include <string>

#include "interfaces/IMatchPlugin.h"
#include "interfaces/CalibInterface.h"
#include "interfaces/HeightPluginInterface.h"
#include "Scene/ImageSceneBase.h"
#include <QMessageBox>
#include <QLabel>

#include "interfaces/orionvisionglobal.h"

class QToolButton;
class QGraphicsView;
class QScrollArea;
class QGroupBox;
class QTextEdit;
class QLabel;
class QPluginLoader;
class CustomTitleBar;

class MainWindow : public QWidget {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    static MainWindow* getInstance();
    static void logAndShowBox(QWidget* parent, const QString& title, const QString& text, QMessageBox::Icon icon = QMessageBox::Information);

public slots:
    void appendLog(const QString& text);
    void CollectBtnClicked();
    void MatchBtnClicked();
    void CalibBtnClicked();
    void OpenCameraBtnClicked();
    void updateStatusLabels();

private:
    void init();
    void loadPlugin(); // 加载插件

    void updateCameraFrame();
    void usbCamera();
    void loadCalibrationData(); // 加载标定数据
    void loadHeightPlugin(); // 加载测高插件
    void loadCalibPlugin(); // 加载相机标定插件

private:
    IMatchWidget* m_matchWidget = nullptr;
    QPluginLoader* m_MatchpluginLoader = nullptr;
    IMatchPluginFactory* m_matchPluginFactory = nullptr;

    QWidget* m_heightMainWindow = nullptr;
    HeightPluginInterface* m_heightPluginFactory = nullptr;

    QWidget* m_CalibMainWindow = nullptr;
    CalibInterface* m_CalibPluginFactory = nullptr;

    QGroupBox* m_stateGroupBox;
    QToolButton* m_CameraCalibBtn;
    QToolButton* m_testHeightBtn;
    QToolButton* m_MatchBtn;
    QToolButton* m_CollectBtn;
    QToolButton* m_OpenCameraBtn;
    QToolButton* m_MirrorcalibBtn;

    ImageSceneBase* m_imageDisplayWidget;
    QTextEdit* m_infoArea;

    QLabel* CamreConnectResultLabel;
    QLabel* CalibResultLabel;
    QLabel* TestHeightResultLabel;
    QLabel* MatchLearnResultLabel;

    QImage m_currentImage;

    cv::VideoCapture cap;
    std::vector<unsigned char> m_pDataForRGB;
    QTimer* m_cameraTimer = nullptr;

    // 标定相关变量
    cv::Mat m_cameraMatrix;
    cv::Mat m_distCoeffs;
    cv::Mat m_map1, m_map2;
    bool m_isCameraCalibLoaded = false;
    cv::Size m_calibImageSize;

    QRect roi_3x3 = QRect(0, 0, 0, 0); // 3x3 ROI，默认中心区域

    cv::Mat HomographyMatrix; // 用于存储单应性矩阵
    bool m_is9_9CalibLoadedr = false;
    bool m_is3_3CalibLoaded = false;

    static MainWindow* s_instance;
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);


    QPointF centralPointF;  // 选中图元的中心坐标
    CopyItems_Move_Rotate_Data m_ItemsMoveRotateData;
};