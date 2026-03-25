#include "CameraWarpectiveWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QApplication>
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include <QStandardPaths>
#include <QDir>
#include <QtGlobal>
#include "../../../../common/tools/bscvTool.h"

using namespace std;
using namespace cv;

namespace {

QtMessageHandler g_prevQtHandler = nullptr;
QTextEdit* g_logTarget = nullptr;

void cameraWarpMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    // Keep original behavior so logs still appear in console/output window.
    if (g_prevQtHandler)
        g_prevQtHandler(type, context, msg);

    if (!g_logTarget)
        return;

    if (!msg.contains("[CameraCorrector]"))
        return;

    const QString line = msg.trimmed();
    if (line.isEmpty())
        return;

    if (QThread::currentThread() == g_logTarget->thread())
        g_logTarget->append(line);
    else
        QMetaObject::invokeMethod(g_logTarget, "append", Qt::QueuedConnection, Q_ARG(QString, line));
}

class ScopedQtLogCapture
{
public:
    explicit ScopedQtLogCapture(QTextEdit* target)
    {
        g_logTarget = target;
        g_prevQtHandler = qInstallMessageHandler(cameraWarpMessageHandler);
    }

    ~ScopedQtLogCapture()
    {
        qInstallMessageHandler(g_prevQtHandler);
        g_prevQtHandler = nullptr;
        g_logTarget = nullptr;
    }
};

} // namespace

CameraWarpectiveWidget::CameraWarpectiveWidget(QWidget *parent)
    : baseWidget(parent), m_scene(nullptr), m_view(nullptr), m_CameraCorrector(nullptr)
{
    init();
    this->setWindowTitle(QStringLiteral("")); 
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    this->sizePolicy().setHorizontalStretch(0);
    this->sizePolicy().setVerticalStretch(0);
    this->resize(1200, 800);
    cv::Mat m = cv::Mat::zeros(3,3,CV_8UC1);
}

CameraWarpectiveWidget::~CameraWarpectiveWidget()
{
}

void CameraWarpectiveWidget::init()
{
    QGridLayout* contentLayout = new QGridLayout();
    QWidget* LeftWidget = new QWidget(contentLayout->widget());
    QGridLayout* layoutSettings = new QGridLayout(LeftWidget); // 改为网格布局，使左侧变窄

    layoutSettings->addWidget(new QLabel("角点列数:"), 0, 0);
    m_editBoardWidth = new QLineEdit("12"); // 默认值设为 12
    layoutSettings->addWidget(m_editBoardWidth, 0, 1);

    layoutSettings->addWidget(new QLabel("角点行数:"), 1, 0);
    m_editBoardHeight = new QLineEdit("9"); // 默认值设为 9
    layoutSettings->addWidget(m_editBoardHeight, 1, 1);

    layoutSettings->addWidget(new QLabel("方格尺寸:"), 2, 0);
    m_editSquareSize = new QLineEdit("6.0"); // 默认值设为 6.0
    m_editSquareSize->setToolTip("(mm)");
    layoutSettings->addWidget(m_editSquareSize, 2, 1);

    layoutSettings->addWidget(new QLabel("图片实际宽度:"), 3, 0);
    m_editOutWidth = new QLineEdit("110"); // 默认值设为 2.5
    m_editOutWidth->setToolTip("输出图片宽度(mm)");
    layoutSettings->addWidget(m_editOutWidth, 3, 1);

    layoutSettings->addWidget(new QLabel("图片实际高度:"), 4, 0);
    m_editOutHeight = new QLineEdit("110"); // 默认为空
    m_editOutHeight->setToolTip("输出图片高度(mm)");
    layoutSettings->addWidget(m_editOutHeight, 4, 1);

    layoutSettings->addWidget(new QLabel("输出分辨率:"), 5, 0);
    m_editpixPerMm = new QLineEdit("10"); // 默认为空
    m_editpixPerMm->setToolTip("输出图片高度(mm)");
    layoutSettings->addWidget(m_editpixPerMm, 5, 1);

    // --- 中间图像显示 ---
    QWidget* imageDisplayWidget = new QWidget(this);
    imageDisplayWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // 显式设置 Expanding

    m_scene = new WarpectiveScene(imageDisplayWidget);
    m_view = new QGraphicsView(imageDisplayWidget);
    m_view->setMouseTracking(true); // 启用鼠标跟踪
    m_view->setInteractive(true); // 启用交互
    m_scene->attachView(m_view); // 让场景附加到视图，启用缩放等行为
    m_view->setStyleSheet("background-color: rgb(40, 40, 40);border:1px solid gray;");
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QHBoxLayout* layoutScene = new QHBoxLayout(imageDisplayWidget);
    layoutScene->setContentsMargins(0, 0, 0, 0);
    layoutScene->setSpacing(0);
    layoutScene->addWidget(m_view);

    QVBoxLayout* layoutImages = new QVBoxLayout();

    QHBoxLayout* layoutImgBtns = new QHBoxLayout();
    m_btnOpenImages = new QPushButton("打开图片", this);
    m_btnChangeShow = new QPushButton("切换显示", this);
    layoutImgBtns->addWidget(m_btnOpenImages);
    layoutImgBtns->addWidget(m_btnChangeShow);
    layoutImages->addLayout(layoutImgBtns);

    QHBoxLayout* btnlayout = new QHBoxLayout();
    m_btnCalibrate = new QPushButton("开始变换", this);
    btnlayout->addWidget(m_btnCalibrate);

    // 保存按钮
    m_btnSave = new QPushButton("保存结果", this);
    btnlayout->addWidget(m_btnSave);
    layoutImages->addLayout(btnlayout);
    QPushButton* test = new QPushButton("测试结果", this);
    btnlayout->addWidget(test);
    connect(test, &QPushButton::clicked, this, &CameraWarpectiveWidget::onTestClicked);

    layoutSettings->addLayout(layoutImages, 6, 0, 1, 2);
    layoutSettings->addWidget(test, 7, 0, 1, 2);
    layoutSettings->setRowStretch(8, 1); // 最后一行占满剩余空间
    QGroupBox* groupResult = new QGroupBox("标定结果", this);
    QVBoxLayout* layoutResult = new QVBoxLayout(groupResult);
    m_textResult = new QTextEdit(this);
    m_textResult->setReadOnly(true); // 设置为只读，用户不可编辑
    layoutResult->addWidget(m_textResult);
    
    contentLayout->addWidget(LeftWidget, 0, 0, 16, 2);
    contentLayout->addWidget(imageDisplayWidget, 0, 2, 16, 10);
    contentLayout->addWidget(groupResult, 16, 0, 4, 12);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 0, 5, 0);
    mainLayout->setSpacing(0);
    mainLayout->addLayout(contentLayout);

    
    connect(m_btnOpenImages, &QPushButton::clicked, this, &CameraWarpectiveWidget::onOpenImagesClicked);
    connect(m_btnChangeShow, &QPushButton::clicked, this, &CameraWarpectiveWidget::onChangeShowClicked);
    connect(m_btnCalibrate, &QPushButton::clicked, this, &CameraWarpectiveWidget::onCalibrateClicked);
    connect(m_btnSave, &QPushButton::clicked, this, &CameraWarpectiveWidget::onSaveClicked);

}

bool CameraWarpectiveWidget::onOpenImagesClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择图像", "", "Images (*.png *.jpg *.bmp)");
    if (fileName.isEmpty()) return false;
    std::string fileStr = fileName.toLocal8Bit().constData();
    m_currentImage = cv::imread(fileStr);
    QImage img;
    TIGER_BSVISION::cvImage2qImage(img, m_currentImage);
    m_scene->setOriginalPixmap(QPixmap::fromImage(img));
    m_scene->resetImage();
    isCurrentImageShow = true;
    return true;
}

void CameraWarpectiveWidget::onChangeShowClicked()
{
    if(isCurrentImageShow) {
        if (m_rectifiedImage.empty())return;
        QImage img;
        TIGER_BSVISION::cvImage2qImage(img, m_rectifiedImage);
        m_scene->setOriginalPixmap(QPixmap::fromImage(img));
        m_scene->resetImage();
        isCurrentImageShow = false;
    } else {
        if (m_currentImage.empty()) return;
        QImage img;
        TIGER_BSVISION::cvImage2qImage(img, m_currentImage);
        m_scene->setOriginalPixmap(QPixmap::fromImage(img));
        m_scene->resetImage();
        isCurrentImageShow = true;
    }
}

void CameraWarpectiveWidget::onCalibrateClicked()
{
    bool ok;
    int gridCols = m_editBoardWidth->text().toInt(&ok);
    if (!ok || gridCols <= 0) {
        m_textResult->append("请输入有效的角点列数");
        return;
    }
    
    int gridRows = m_editBoardHeight->text().toInt(&ok);
    if (!ok || gridRows <= 0) {
        m_textResult->append("请输入有效的角点行数");
        return;
    }

    float squareMm = m_editSquareSize->text().toFloat(&ok);
    if (!ok || squareMm <= 0) {
        m_textResult->append("请输入有效的方格尺寸");
        return;
    }

    int outWidth = m_editOutWidth->text().toInt(&ok);
    if (!ok || outWidth <= 0) {
        m_textResult->append("请输入有效的输出图片宽度");
        return;
    }

    int outHeight = m_editOutHeight->text().toInt(&ok);
    if (!ok || outHeight <= 0) {
        m_textResult->append("请输入有效的输出图片高度");
        return;
    }

    float pixPerMm = m_editpixPerMm->text().toFloat(&ok);
    if (!ok || pixPerMm <= 0) {
        m_textResult->append("请输入有效的输出分辨率");
        return;
    }
    if(m_currentImage.empty()) {
        m_textResult->append("请先打开一张图片");
        return;
    }

    m_config.gridCols = gridCols;
    m_config.gridRows = gridRows;
    m_config.squareMm = squareMm;
    m_config.outWidthMm = outWidth;
    m_config.outHeightMm = outHeight;
    m_config.pixPerMm = pixPerMm;

    if(m_CameraCorrector == nullptr) {
        m_CameraCorrector = new CameraCorrector(m_config);
    } else {
        delete m_CameraCorrector;
        m_CameraCorrector = new CameraCorrector(m_config);
    }

    m_textResult->append("正在变换中，请稍候...");
    // 强制刷新界面，显示提示信息，防止界面卡死
    QApplication::processEvents(); 

    try {
        ScopedQtLogCapture capture(m_textResult);

        if(!m_CameraCorrector->computeMap(m_currentImage)){
            cerr << "Error: Failed to compute homography map" << endl;
            m_textResult->append("Error: Failed to compute homography map");
            return;
        }
        isCalibrated = true;
        if(!m_CameraCorrector->warp(m_currentImage, m_rectifiedImage)){
            cerr << "Error: Failed to warp image" << endl;
            m_textResult->append("Error: Failed to warp image");
            return;
        }
        onChangeShowClicked(); // 切换显示校正后的图像
    } catch (const std::exception& e) {
        cerr << "Error during rectification: " << e.what() << endl;
        m_textResult->append(QString("Error during rectification: %1").arg(e.what()));
    }

}

void CameraWarpectiveWidget::onSaveClicked()
{
    if(!isCalibrated) {
        m_textResult->append("请先完成标定");
        return;
    }
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QDir().mkpath(desktopPath);
    QString defaultName = QDir(desktopPath).filePath("homography.yml");
    QString fileName = QFileDialog::getSaveFileName(this, tr("结果"), defaultName,
                                                    tr("YAML 文件 (*.yml)"));
    if (fileName.isEmpty()) return;
    std::string fileStr = fileName.toStdString();
    m_CameraCorrector->save(fileStr);
}

void CameraWarpectiveWidget::onTestClicked()
{
    if(!onOpenImagesClicked()) { // 重新打开原图
        m_textResult->append("未打开图像");
        return;
    }
    if(m_CameraCorrector == nullptr) {
        m_CameraCorrector = new CameraCorrector(m_config);
    } else {
        delete m_CameraCorrector;
        m_CameraCorrector = new CameraCorrector(m_config);
    }
    QString fileName = QFileDialog::getOpenFileName(this, "选择结果", "", "yml (*.yml) *.XML");
    m_CameraCorrector->load(fileName.toStdString()); // 从文件加载单应矩阵
    m_CameraCorrector->warp(m_currentImage, m_rectifiedImage); // 重新变换图像
    onChangeShowClicked(); // 切换显示校正后的图像
}
