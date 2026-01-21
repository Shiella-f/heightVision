#include "MainWindow.h"
#include "common/Widget/CustomTitleBar.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QTextEdit>
#include <QGraphicsView>
#include <QGroupBox>
#include <QToolButton>
#include <QLabel>
#include <QTimer>
#include <QDebug>
#include <QImage>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include "tools/bscvTool.h"
#include <QPluginLoader>
#include <QApplication>
#include <QCameraInfo>


MainWindow* MainWindow::s_instance = nullptr;

void MainWindow::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (!s_instance) return;

    QString txt;
    switch (type) {
    case QtDebugMsg:
        txt = QString("[Debug] %1").arg(msg);
        break;
    case QtWarningMsg:
        txt = QString("[Warning] %1").arg(msg);
        break;
    case QtCriticalMsg:
        txt = QString("[Critical] %1").arg(msg);
        break;
    case QtFatalMsg:
        txt = QString("[Fatal] %1").arg(msg);
        break;
    case QtInfoMsg:
        txt = QString("[Info] %1").arg(msg);
        break;
    }

    // 确保在主线程更新 UI
    QMetaObject::invokeMethod(s_instance, "appendLog", Qt::QueuedConnection, Q_ARG(QString, txt));
}

const QString buttonStyle = QStringLiteral( 
        "QToolButton{border-radius: 5px;solid gray;color:black;text-align:center;}"
        "QToolButton:hover{border:1px solid black;background-color:rgb(200,200,200);color:black;}"
        "QToolButton:pressed{background-color:rgb(150,150,150);color:white;}"
        "QToolButton:disabled{background-color:rgb(40,40,40);color:white;}"
    );

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent), m_matchWidget(nullptr), m_heightMainWindow(nullptr), m_CalibMainWindow(nullptr), m_stateGroupBox(nullptr),
      m_CameraCalibBtn(nullptr), m_MirrorcalibBtn(nullptr), m_testHeightBtn(nullptr), m_MatchBtn(nullptr),
      m_CollectBtn(nullptr), m_imageDisplayWidget(nullptr), m_infoArea(nullptr)
{
    s_instance = this;
    qInstallMessageHandler(MainWindow::messageHandler);

    centralPointF = QPointF(100.0,100.0);  // 选中图元的中心坐标
    m_ItemsMoveRotateData = CopyItems_Move_Rotate_Data();

    // 设置无边框窗口
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);

        // 创建自定义标题栏
    CustomTitleBar* titleBar = new CustomTitleBar(this);
    titleBar->setTitle("Lpcv Tool");
    connect(titleBar, &CustomTitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(titleBar, &CustomTitleBar::closeClicked, this, &QWidget::close);

    init();
    loadPlugin();
    loadHeightPlugin(); // 加载测高插件
    loadCalibPlugin(); // 加载相机标定插件
    this->setWindowTitle(QStringLiteral("Lpcv Tool"));
    this->setFixedSize(800, 800);
}

void MainWindow::init() 
{
    QLabel* CamreConnectLabel = new QLabel(this);
    CamreConnectLabel->setText(QStringLiteral("相机连接"));
    CamreConnectResultLabel = new QLabel(this);
    CamreConnectResultLabel->setText(QString("NO"));

    QLabel* CalibLabel = new QLabel(this);
    CalibLabel->setText(QStringLiteral("标定"));
    CalibResultLabel = new QLabel(this);
    CalibResultLabel->setText(QString("NO"));

    QLabel* TestHeightLabel = new QLabel(this);
    TestHeightLabel->setText(QStringLiteral("测高"));
    TestHeightResultLabel = new QLabel(this);
    TestHeightResultLabel->setText(QString("NO"));

    QLabel* MatchLearnLabel = new QLabel(this);
    MatchLearnLabel->setText(QStringLiteral("模板学习"));
    MatchLearnResultLabel = new QLabel(this);
    MatchLearnResultLabel->setText(QString("NO"));

    QHBoxLayout* stateLayout = new QHBoxLayout;
    stateLayout->setContentsMargins(5, 5, 5, 5);
    stateLayout->addWidget(CamreConnectLabel);
    stateLayout->addWidget(CamreConnectResultLabel);
    stateLayout->addWidget(CalibLabel);
    stateLayout->addWidget(CalibResultLabel);
    stateLayout->addWidget(TestHeightLabel);
    stateLayout->addWidget(TestHeightResultLabel);
    stateLayout->addWidget(MatchLearnLabel);
    stateLayout->addWidget(MatchLearnResultLabel);

    m_stateGroupBox = new QGroupBox(QStringLiteral("状态"), this);
    m_stateGroupBox->setLayout(stateLayout);

    auto newButton = [](QToolButton* btn, const QString& text) {
        btn->setText(text);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        btn->setStyleSheet(buttonStyle);
        btn->setFixedSize(80, 20);
        return btn;
    };
    m_CameraCalibBtn = newButton(new QToolButton(this), QStringLiteral("标定"));
    connect(m_CameraCalibBtn, &QToolButton::clicked, this, &MainWindow::CalibBtnClicked);
    m_testHeightBtn = newButton(new QToolButton(this), QStringLiteral("测高"));
    connect(m_testHeightBtn, &QToolButton::clicked, this, [this]() {
        if (m_heightMainWindow) {
            m_heightMainWindow->show();
            m_heightMainWindow->raise();
            m_heightMainWindow->activateWindow();
        }else if(m_heightPluginFactory){
            m_heightMainWindow = m_heightPluginFactory->createWidget(nullptr);
            m_heightMainWindow->show();
        }else{
            loadHeightPlugin();
            if(m_heightPluginFactory){
                m_heightMainWindow = m_heightPluginFactory->createWidget(nullptr);
                m_heightMainWindow->show();
                m_heightMainWindow->raise();
                m_heightMainWindow->activateWindow();
        }else{
            QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("测高插件加载失败！"));
        }
        }
    });

    m_CollectBtn = newButton(new QToolButton(this), QStringLiteral("采集"));
    connect(m_CollectBtn, &QToolButton::clicked, this, &MainWindow::CollectBtnClicked);
    m_MatchBtn = newButton(new QToolButton(this), QStringLiteral("匹配"));
    connect(m_MatchBtn, &QToolButton::clicked, this, &MainWindow::MatchBtnClicked);
    m_OpenCameraBtn = newButton(new QToolButton(this), QStringLiteral("打开相机"));
    connect(m_OpenCameraBtn, &QToolButton::clicked, this, &MainWindow::OpenCameraBtnClicked);

    QHBoxLayout* BtnLayout = new QHBoxLayout;
    BtnLayout->setContentsMargins(5, 5, 5, 5);
    BtnLayout->addWidget(m_CameraCalibBtn);
    BtnLayout->addWidget(m_testHeightBtn);
    BtnLayout->addWidget(m_CollectBtn);
    BtnLayout->addWidget(m_MatchBtn);
    BtnLayout->addWidget(m_OpenCameraBtn);
    BtnLayout->addWidget(m_MirrorcalibBtn);

    QWidget* UpWidget = new QWidget(this);
    QHBoxLayout* UpLayout = new QHBoxLayout;
    QSpacerItem* spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    UpLayout->setContentsMargins(0, 0, 0, 0);
    UpLayout->addLayout(BtnLayout);
    UpLayout->addItem(spacer);
    UpLayout->addWidget(m_stateGroupBox);
    UpWidget->setLayout(UpLayout);

    QGraphicsView* imageView = new QGraphicsView(this); // 主图像视图
    imageView->setAlignment(Qt::AlignCenter);
    imageView->resize(780, 520); // 初始大小，保持 3072x2048 比例
    imageView->setFixedSize(780, 520);
    imageView->setMouseTracking(true); // 启用鼠标跟踪
    imageView->setInteractive(true);
    m_imageDisplayWidget = new ImageSceneBase(imageView);
    m_imageDisplayWidget->attachView(imageView); // 让场景附加到视图，启用缩放等行为
    imageView->setStyleSheet("background-color: rgb(40, 40, 40);border:1px solid gray;");
    imageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QGroupBox* groupResult = new QGroupBox(QStringLiteral("信息日志"), this);
    QVBoxLayout* resultLayout = new QVBoxLayout(groupResult);
    m_infoArea = new QTextEdit(groupResult);
    m_infoArea->setReadOnly(true); // 设置为只读，用户不可编辑
    resultLayout->addWidget(m_infoArea);
    groupResult->setLayout(resultLayout);

    // 创建自定义标题栏
    CustomTitleBar* titleBar = new CustomTitleBar(this);
    titleBar->setTitle("Lpcv Tool");
    connect(titleBar, &CustomTitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(titleBar, &CustomTitleBar::closeClicked, this, &QWidget::close);

    // 内容区域容器
    QWidget* contentWidget = new QWidget(this);
    QGridLayout* mainLayout = new QGridLayout(contentWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->addWidget(UpWidget, 0, 0, 1, 1);
    mainLayout->addWidget(imageView, 1, 0, 8, 2);
    mainLayout->addWidget(groupResult, 9, 0, 2, 2);
    
    // 主布局（包含标题栏和内容区域）
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(titleBar);
    rootLayout->addWidget(contentWidget);
    setLayout(rootLayout);

    QTimer* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusLabels);
    statusTimer->start(500);


}

void MainWindow::loadPlugin()
{
    // 获取应用程序目录
    QDir pluginsDir(QCoreApplication::applicationDirPath());
    
    // 尝试加载 TemplateMatchPlugin
    QString pluginName = "TemplateMatchPlugin";
    
    // 使用 QPluginLoader 加载
    m_MatchpluginLoader = new QPluginLoader(pluginsDir.absoluteFilePath(pluginName), this);
    QObject *plugin = m_MatchpluginLoader->instance();
    
    if (plugin) {
        // 尝试转换为工厂接口
        m_matchPluginFactory = qobject_cast<IMatchPluginFactory *>(plugin);
        if (m_matchPluginFactory) {
            m_matchWidget = m_matchPluginFactory->createMatchWidget(nullptr);
            appendLog("TemplateMatchPlugin loaded successfully.");
        } else {
            appendLog("TemplateMatchPlugin loaded but interface mismatch.");
            delete m_MatchpluginLoader;
            m_MatchpluginLoader = nullptr;
        }
    } else {
        appendLog("Failed to load TemplateMatchPlugin: " + m_MatchpluginLoader->errorString());
    }
}

MainWindow::~MainWindow()
{
    qInstallMessageHandler(nullptr); // 恢复默认处理程序
    s_instance = nullptr;

    if (m_cameraTimer) {
        m_cameraTimer->stop();
        delete m_cameraTimer;
        m_cameraTimer = nullptr;
    }
    if (m_matchWidget) {
        delete m_matchWidget->asWidget();
        m_matchWidget = nullptr;
    }
    if(m_heightMainWindow) {
        delete m_heightMainWindow;
        m_heightMainWindow = nullptr;
    }
    if(m_CalibMainWindow) {
        delete m_CalibMainWindow;
        m_CalibMainWindow = nullptr;
    }
    if(m_MatchpluginLoader) {
        delete m_MatchpluginLoader;
        m_MatchpluginLoader = nullptr;
    }
}

MainWindow* MainWindow::getInstance()
{
    return s_instance;
}

void MainWindow::logAndShowBox(QWidget* parent, const QString& title, const QString& text, QMessageBox::Icon icon)
{
    // 先记录日志（会被 messageHandler 捕获并显示在 m_infoArea）
    QString typeStr;
    switch (icon) {
        case QMessageBox::Information: typeStr = "Info"; break;
        case QMessageBox::Warning: typeStr = "Warning"; break;
        case QMessageBox::Critical: typeStr = "Critical"; break;
        case QMessageBox::Question: typeStr = "Question"; break;
        default: typeStr = "Message"; break;
    }
    qInfo() << QString("[%1 Box] Title: %2, Content: %3").arg(typeStr, title, text);

    // 再显示弹窗
    QMessageBox msgBox(icon, title, text, QMessageBox::Ok, parent);
    msgBox.exec();
}

void MainWindow::appendLog(const QString& text)
{
    if (m_infoArea) {
        m_infoArea->append(text);
    }
}

void MainWindow::usbCamera()
{
    cap.open(1); // 打开默认摄像头
    if (!cap.isOpened()) {
        m_infoArea->append("无法打开摄像头");
        return;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
    m_infoArea->append("USB Camera opened successfully.");
    bool fpsOk = cap.set(cv::CAP_PROP_FPS, 15);
    cap.set(cv::CAP_PROP_AUTOFOCUS, 0);
    cap.set(cv::CAP_PROP_FOCUS, 350);
    m_cameraTimer = new QTimer(this);
    connect(m_cameraTimer, &QTimer::timeout, this, &MainWindow::updateCameraFrame);
    m_cameraTimer->start(66);
}

void MainWindow::updateCameraFrame()
{
    if(cap.isOpened())
    {
        cv::Mat frame;
        cap >> frame; // 从摄像头捕获一帧
        if (frame.empty()) {
            QImage qimg(640, 480, QImage::Format_RGB888);
            qimg.fill(Qt::black);
            if (m_imageDisplayWidget) {
                m_imageDisplayWidget->setOriginalPixmap(QPixmap::fromImage(qimg));
            }
        } else {
            // 2. 如果已加载标定数据，进行去畸变
            cv::Mat displayedFrame = frame;
            if (m_isCameraCalibLoaded) {
                // 如果图像尺寸改变，需要重新初始化映射表
                if (frame.size() != m_calibImageSize) {
                    m_calibImageSize = frame.size();
                    // 计算新的相机矩阵，alpha=0 裁剪黑边，alpha=1 保留所有像素
                    cv::Mat newCameraMatrix = cv::getOptimalNewCameraMatrix(m_cameraMatrix, m_distCoeffs, m_calibImageSize, 1, m_calibImageSize, 0);
                    cv::initUndistortRectifyMap(m_cameraMatrix, m_distCoeffs, cv::Mat(), newCameraMatrix, m_calibImageSize, CV_16SC2, m_map1, m_map2);
                }
                
                cv::Mat undistortedFrame;
                cv::remap(frame, undistortedFrame, m_map1, m_map2, cv::INTER_LINEAR);
                if(roi_3x3.width() > 0 && roi_3x3.height() > 0 &&
                   roi_3x3.x() >= 0 && roi_3x3.y() >= 0 &&
                   roi_3x3.x() + roi_3x3.width() <= undistortedFrame.cols &&
                   roi_3x3.y() + roi_3x3.height() <= undistortedFrame.rows) 
                {
                    displayedFrame = undistortedFrame( cv::Rect(roi_3x3.x(), roi_3x3.y(), roi_3x3.width(), roi_3x3.height()) ).clone();
                } else {
                    displayedFrame = undistortedFrame;
                }

            }

            // 3. 转换颜色并显示在主界面 (显示去畸变后的图像)
            cv::cvtColor(displayedFrame, displayedFrame, cv::COLOR_BGR2RGB);
            QImage qimg(displayedFrame.data, displayedFrame.cols, displayedFrame.rows, displayedFrame.step, QImage::Format_RGB888);
            if (m_imageDisplayWidget) 
            {
                m_imageDisplayWidget->setOriginalPixmap(QPixmap::fromImage(qimg));
            }
        }
    } else {
        m_infoArea->append("Camera handle or VideoCapture not initialized.");
    }
}

void MainWindow::loadCalibrationData()
{
    int ret1 = QMessageBox::question(this,
                                     "相机标定",
                                     "是否选择相机标定文件？",
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    QString calibPath = nullptr;
    if (ret1 == QMessageBox::Yes) {
        calibPath = QFileDialog::getOpenFileName(nullptr, ("选择相机标定文件"), "", ("Images (*.xml);;All Files (*)"));
    }
    QFile file(calibPath);
    if (!file.exists()) {
        m_infoArea->append("相机标定文件不存在或未选择标定文件");
        return;
    }

    try {
        cv::FileStorage fs(calibPath.toStdString(), cv::FileStorage::READ);
        if (!fs.isOpened()) {
            m_infoArea->append("无法打开相机标定文件");
            return;
        }

        fs["camera_matrix"] >> m_cameraMatrix;
        fs["distortion_coefficients"] >> m_distCoeffs;
        fs.release();

        if (m_cameraMatrix.empty() || m_distCoeffs.empty()) {
            m_infoArea->append("相机标定文件数据无效");
            m_isCameraCalibLoaded = false;
        } else {
            m_isCameraCalibLoaded = true;
            m_infoArea->append("相机标定数据加载成功！");
        }
    } catch (cv::Exception& e) {
        m_infoArea->append(QString("加载标定数据异常: %1").arg(e.what()));
        m_isCameraCalibLoaded = false;
    }


    int ret2 = QMessageBox::question(this,
                                     "振镜标定",
                                     "是否选择3*3振镜标定文件(ROI)？",
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    QString calibPath2;
    if (ret2 == QMessageBox::Yes) {
        calibPath2 = QFileDialog::getOpenFileName(nullptr, ("选择3*3振镜标定文件(ROI)"), "", ("Text Files (*.txt);;All Files (*)"));
    }
    
    if (!calibPath2.isEmpty()) {
        QFile roiFile(calibPath2);
        if (!roiFile.exists() || !roiFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_infoArea->append("3*3振镜标定文件不存在或无法打开");
        } else {
            QTextStream in(&roiFile);
            int x = 0, y = 0, w = 0, h = 0;
            bool xFound = false, yFound = false, wFound = false, hFound = false;

            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith("X=")) {
                    x = line.mid(2).toInt();
                    xFound = true;
                } else if (line.startsWith("Y=")) {
                    y = line.mid(2).toInt();
                    yFound = true;
                } else if (line.startsWith("Width=")) {
                    w = line.mid(6).toInt();
                    wFound = true;
                } else if (line.startsWith("Height=")) {
                    h = line.mid(7).toInt();
                    hFound = true;
                }
            }
            roiFile.close();

            if (xFound && yFound && wFound && hFound) {
                roi_3x3 = QRect(x, y, w, h);
                m_infoArea->append(QString("3*3振镜ROI加载成功: X=%1, Y=%2, W=%3, H=%4").arg(x).arg(y).arg(w).arg(h));
                m_is3_3CalibLoaded = true;
            } else {
                m_infoArea->append("3*3振镜标定文件格式错误，未找到完整的ROI信息");
                m_is3_3CalibLoaded = false;
            }
        }
    }

    int ret3 = QMessageBox::question(this,
                                     "9*9振镜标定",
                                     "是否选择9*9振镜标定文件？",
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    QString calibMatrixPath = nullptr;
    if (ret3 == QMessageBox::Yes) {
        calibMatrixPath = QFileDialog::getOpenFileName(nullptr, ("选择9*9振镜标定文件"), "", ("Images (*.xml);;All Files (*)"));
    }
    QFile Matrixfile(calibMatrixPath);
    if (!Matrixfile.exists()) {
        m_infoArea->append("9*9振镜标定文件不存在或未选择标定文件");
        return;
    }

    try {
        cv::FileStorage fs(calibPath.toStdString(), cv::FileStorage::READ);
        if (!fs.isOpened()) {
            m_infoArea->append("无法打开9*9振镜标定文件");
            return;
        }

        fs["HomographyMatrix"] >> HomographyMatrix;
        fs.release();

        if (HomographyMatrix.empty()) {
            m_infoArea->append("9*9振镜标定文件数据无效");
            m_is9_9CalibLoadedr = false;
        } else {
            m_is9_9CalibLoadedr = true;
            m_infoArea->append("9*9振镜标定数据加载成功！");
        }
    } catch (cv::Exception& e) {
        m_infoArea->append(QString("加载标定数据异常: %1").arg(e.what()));
        m_is9_9CalibLoadedr = false;
    }

}

void MainWindow::CollectBtnClicked()
{
    m_currentImage = m_imageDisplayWidget->getSourceImageMat();
    if (m_currentImage.isNull()) {
        m_infoArea->append("当前没有图像可保存");
        return;
    }

    // 弹出保存对话框，默认文件名为时间戳 PNG
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString dirPath = QDir(desktopPath).filePath("bcadhicv/res/UsbCameraCalibimg/usb800w");
    QDir().mkpath(dirPath);
    QString defaultName = QDir(dirPath).filePath("camera_capture_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png");
    QString fileName = QFileDialog::getSaveFileName(this, tr("保存图像"), defaultName,
                                                    tr("PNG 图像 (*.png);;JPEG 图像 (*.jpg *.jpeg);;Bitmap 图像 (*.bmp)"));
    if (fileName.isEmpty()) return;

    // 保存图像（QImage::save 会根据后缀自动选择格式）
    bool ok = m_currentImage.save(fileName);
    if (!ok) {
        m_infoArea->append(QString("保存图像失败：%1").arg(fileName));
    } else {
        m_infoArea->append(QString("图像已保存到：%1").arg(fileName));
    }
}

void MainWindow::MatchBtnClicked()
{
    QWidget *parentWidget;
    // 如果插件未加载，尝试重新加载
    if (!m_matchPluginFactory) {
        loadPlugin();
        if (!m_matchPluginFactory) {
            QMessageBox::warning(this, "Error", "Template Match Plugin not loaded.\n" + (m_MatchpluginLoader ? m_MatchpluginLoader->errorString() : "Unknown error"));
            return;
        }
    }

    if (m_matchWidget) {
        m_matchWidget->show();
    } else {
        // 使用工厂创建实例
        m_matchWidget = m_matchPluginFactory->createMatchWidget(nullptr);
        if (m_matchWidget) {
            m_matchWidget->show();
        } else {
            QMessageBox::warning(this, "Error", "Failed to create Match Widget.");
            return;
        }
    }

    cv::Mat currentMat;
    if(m_currentImage.isNull())
    {
        m_currentImage = m_imageDisplayWidget->getSourceImageMat();
    }
    TIGER_BSVISION::qImage2cvImage(m_currentImage, currentMat);
    
    if (m_matchWidget) {
        m_matchWidget->setcurrentImage(currentMat);
    }
}

void MainWindow::CalibBtnClicked()
{
    if (m_CalibMainWindow != nullptr) 
    {
        m_CalibMainWindow->show();
        m_CalibMainWindow->raise();
        m_CalibMainWindow->activateWindow();
    }else if(m_CalibPluginFactory)
    {
        m_CalibMainWindow = m_CalibPluginFactory->createWidget(nullptr);
        m_CalibMainWindow->show();
    }else{
        loadCalibPlugin();
        if(m_CalibPluginFactory)
        {
             m_CalibMainWindow = m_CalibPluginFactory->createWidget(nullptr);
            m_CalibMainWindow->show();
            m_CalibMainWindow->raise();
            m_CalibMainWindow->activateWindow();
        }else {
            QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("相机标定插件加载失败！"));
        }
    }
}

void MainWindow::OpenCameraBtnClicked()
{
    loadCalibrationData();

    if (m_cameraTimer) {
        if (m_cameraTimer->isActive()) {
            m_cameraTimer->stop();
        }
        delete m_cameraTimer;
        m_cameraTimer = nullptr;
    }
    if (cap.isOpened()) {
        cap.release();
    }
         usbCamera();
}

void MainWindow::updateStatusLabels()
{
    bool isCameraConnected = cap.isOpened();
    CamreConnectResultLabel->setText(isCameraConnected ? "YES" : "NO");
    CamreConnectResultLabel->setStyleSheet(isCameraConnected ? "color: green;" : "color: red;");

    CalibResultLabel->setText(m_is9_9CalibLoadedr && m_is3_3CalibLoaded && m_isCameraCalibLoaded ? "YES" : "NO");
    CalibResultLabel->setStyleSheet((m_is9_9CalibLoadedr && m_is3_3CalibLoaded && m_isCameraCalibLoaded) ? "color: green;" : "color: red;");

    bool isHeightCalibrated = m_heightMainWindow!=nullptr;
    TestHeightResultLabel->setText(isHeightCalibrated ? "YES" : "NO");
    TestHeightResultLabel->setStyleSheet(isHeightCalibrated ? "color: green;" : "color: red;");

    bool isTemplateLearned = m_matchWidget && m_matchWidget->hasLearnedTemplate();
    MatchLearnResultLabel->setText(isTemplateLearned ? "YES" : "NO");
    MatchLearnResultLabel->setStyleSheet(isTemplateLearned ? "color: green;" : "color: red;");
}

void MainWindow::loadHeightPlugin()
{
     // 获取应用程序目录
    QDir pluginsDir(QCoreApplication::applicationDirPath());
    
    // 尝试加载 TemplateMatchPlugin
    QString pluginName = "HeightMeaturePlugin";
    QStringList paths = {
        pluginsDir.absoluteFilePath(pluginName),
        pluginsDir.absoluteFilePath("plugins/" + pluginName),
        pluginsDir.absoluteFilePath("src/plugins/heightMeature/" + pluginName),
        pluginsDir.absoluteFilePath("../src/plugins/heightMeature/" + pluginName)
    };
    for(const QString& path : paths) {
        // 使用 QPluginLoader 加载
        QPluginLoader loader(path, this);
        QObject *plugin = loader.instance();
        
        if (plugin) {
            // 尝试转换为工厂接口
            m_heightPluginFactory = qobject_cast<HeightPluginInterface *>(plugin);
            if (m_heightPluginFactory) {
                appendLog("HeightMeasurePlugin loaded successfully from: " + path);
                return;
            } else {
                appendLog(QString("Failed to cast plugin from %1 to HeightPluginInterface: ").arg(path) + loader.errorString());
            }
        } else {
            appendLog("Failed to load HeightMeasurePlugin from " + path + ": " + loader.errorString());
            // 不删除 loader，以便后续重试或查看错误
            // Don't delete loader to allow retry or error inspection
        }
    }
    appendLog("HeightMeasurePlugin could not be loaded from any known path.");
}

void MainWindow::loadCalibPlugin()
{
         // 获取应用程序目录
    QDir pluginsDir(QCoreApplication::applicationDirPath());
    
    // 尝试加载 CalibPlugin
    QString pluginName = "CalibPlugin";
    QStringList paths = {
        pluginsDir.absoluteFilePath(pluginName),
        pluginsDir.absoluteFilePath("plugins/" + pluginName),
        pluginsDir.absoluteFilePath("src/plugins/Calib/" + pluginName),
        pluginsDir.absoluteFilePath("../src/plugins/Calib/" + pluginName)
    };
    for(const QString& path : paths) {
        // 使用 QPluginLoader 加载
        QPluginLoader loader(path, this);
        QObject *plugin = loader.instance();
        
        if (plugin) {
            // 尝试转换为工厂接口
            m_CalibPluginFactory = qobject_cast<CalibInterface *>(plugin);
            if (m_CalibPluginFactory) {
                appendLog("CalibPlugin loaded successfully from: " + path);
                return;
            } else {
                appendLog(QString("Failed to cast plugin from %1 to CalibInterface: ").arg(path) + loader.errorString());
            }
        } else {
            appendLog("Failed to load CalibPlugin from " + path + ": " + loader.errorString());
            // 不删除 loader，以便后续重试或查看错误
            // Don't delete loader to allow retry or error inspection
        }
    }
    appendLog("CalibPlugin could not be loaded from any known path.");
}
