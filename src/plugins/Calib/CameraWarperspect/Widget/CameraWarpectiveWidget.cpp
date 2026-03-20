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
#include <QToolTip>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

CameraWarpectiveWidget::CameraWarpectiveWidget(QWidget *parent)
    : baseWidget(parent), m_scene(nullptr), m_view(nullptr)
{
    init();
    this->setWindowTitle(QStringLiteral("")); 
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    this->sizePolicy().setHorizontalStretch(0);
    this->sizePolicy().setVerticalStretch(0);
    this->resize(1200, 800);
    connectAll();
    m_CameraCorrector = CameraCorrector();
    cv::Mat m = cv::Mat::zeros(3,3,CV_8UC1);
}

CameraWarpectiveWidget::~CameraWarpectiveWidget()
{
}

void CameraWarpectiveWidget::init()
{
    QGridLayout* contentLayout = new QGridLayout();
    QWidget* LeftWidget = new QWidget(contentLayout->widget());
    m_OpenImages = new QPushButton("打开图片", LeftWidget);
    m_SwitchSHowBtn = new QPushButton("切换显示", LeftWidget);
    QGridLayout* layoutSettings = new QGridLayout(LeftWidget); // 改为网格布局，使左侧变窄
    layoutSettings->addWidget(m_OpenImages, 0, 0, 1, 2);
    layoutSettings->addWidget(m_SwitchSHowBtn, 0, 2, 1, 2);
    
    QRegExp regex("^[0-9]+(\\.[0-9]+)?,[0-9]+(\\.[0-9]+)?$");

    for(int i = 0; i < 4; ++i) {
        LactionLabel[i] = new QLabel(LeftWidget);
        if(i == 0) LactionLabel[i]->setText("左上角:");
        else if(i == 1) LactionLabel[i]->setText("右上角:");
        else if(i == 2) LactionLabel[i]->setText("右下角:");
        else if(i == 3) LactionLabel[i]->setText("左下角:");
        m_ImgPointLd[i] = new QLineEdit(LeftWidget);
        m_ObjPointLd[i] = new QLineEdit(LeftWidget);
        m_ImgPointLd[i]->setPlaceholderText("请输入图片坐标");
        m_ObjPointLd[i]->setPlaceholderText("请输入实际坐标");
        m_ObjPointLd[i]->setToolTip("示例：123.45,678.90");
        m_ImgPointLd[i]->setToolTip("示例：123.45,678.90");
        QRegExpValidator *validator = new QRegExpValidator(regex, m_ImgPointLd[i]);
        m_ImgPointLd[i]->setValidator(validator);
        QRegExpValidator *validator2 = new QRegExpValidator(regex, m_ObjPointLd[i]);
        m_ObjPointLd[i]->setValidator(validator2);
        QHBoxLayout* pointLayout = new QHBoxLayout();
        pointLayout->addWidget(LactionLabel[i]);
        pointLayout->addWidget(m_ImgPointLd[i]);
        pointLayout->addWidget(m_ObjPointLd[i]);
        layoutSettings->addLayout(pointLayout, i+1, 0, 1, 4); // 占满4列
    }
    m_selectPointsBtn = new QPushButton("选择点", LeftWidget);
    m_EnsurePoint = new QPushButton("确定坐标", LeftWidget);
    m_StartBtn = new QPushButton("开始标定", LeftWidget);
    m_SaveBtn = new QPushButton("保存结果", LeftWidget);
     // 占满2列
    layoutSettings->addWidget(m_selectPointsBtn, 5, 0, 1, 2); // 占满2列
    layoutSettings->addWidget(m_EnsurePoint, 5, 2, 1, 2); // 占满2列

    layoutSettings->addWidget(m_StartBtn, 6, 0, 1, 2); // 占满2列
     // 占满2列
    layoutSettings->addWidget(m_SaveBtn, 6, 2, 1, 2); // 占满4列
    layoutSettings->setContentsMargins(0, 0, 0, 0);
    layoutSettings->setRowStretch(7, 1); // 最后一行占满剩余空间

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
}

void CameraWarpectiveWidget::connectAll(){
    connect(m_OpenImages,&QPushButton::clicked,this,&CameraWarpectiveWidget::onOpenImagesClicked);
    connect(m_selectPointsBtn,&QPushButton::clicked,this,&CameraWarpectiveWidget::onSelectPointsClicked);
    connect(m_EnsurePoint,&QPushButton::clicked,this,&CameraWarpectiveWidget::onEnsurePointClicked);
    connect(m_SaveBtn,&QPushButton::clicked,this,&CameraWarpectiveWidget::onSaveClicked);
    connect(m_scene, &WarpectiveScene::currentPointChanged, this, &CameraWarpectiveWidget::SelectPoints);
    connect(m_StartBtn, &QPushButton::clicked, this, &CameraWarpectiveWidget::onStartCalibClicked);
    connect(m_SwitchSHowBtn, &QPushButton::clicked, this, &CameraWarpectiveWidget::onSwitchShowClicked);
}

void CameraWarpectiveWidget::onOpenImagesClicked()
{
    QString files = QFileDialog::getOpenFileName(
        this,
        "选择标定图片",
        "", // 默认路径
        "Images (*.png *.jpg *.bmp *.jpeg)" // 文件过滤器
    );
    if (files.isEmpty()) return;
    m_currentImage = cv::imread(files.toLocal8Bit().constData());
    if (m_currentImage.empty()) return;
    cv::Mat rgb;
    cv::cvtColor(m_currentImage, rgb, cv::COLOR_BGR2RGB);
    QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    m_scene->setOriginalPixmap(QPixmap::fromImage(img));
    m_scene->resetImage();
    m_showOriginal = true;
}

void CameraWarpectiveWidget::onSaveClicked()
{
    QString savePath = QFileDialog::getSaveFileName(this, "保存标定结果", "tilt_params.yaml", "OpenCV XML (*.xml);;OpenCV YAML (*.yaml);;Text File (*.txt)");
    if(savePath.isEmpty()) return;
    std::string path = savePath.toStdString();
    m_CameraCorrector.saveTiltParams(path);
}

void CameraWarpectiveWidget::onSelectPointsClicked(){
    if(!m_scene) return;
    m_textResult->append("请在图像上选择4个点，顺序为：左上、右上、右下、左下");
    m_scene->setSelectingPoint(true);
}

void CameraWarpectiveWidget::onEnsurePointClicked(){
    
    for(int i = 0; i < 4; ++i){
        m_ImgPoints[i].setX(0.0);
        m_ImgPoints[i].setY(0.0);
        m_ObjPoints[i].setX(0.0);
        m_ObjPoints[i].setY(0.0);
        if(m_ImgPointLd[i]->text().isEmpty()){
            QMessageBox::warning(this, "输入错误", "请确保所有坐标输入框都已填写。");
            return;
        }
        QStringList imgCoords = m_ImgPointLd[i]->text().split(",");
        if (imgCoords.size() == 2) {
            m_ImgPoints[i].setX(imgCoords[0].toDouble());
            m_ImgPoints[i].setY(imgCoords[1].toDouble());
        } else {
            QMessageBox::warning(this, "输入错误", QString("第 %1 个点的坐标格式不正确。").arg(i + 1));
            return;
        }
    }
}

void CameraWarpectiveWidget::SelectPoints(QPointF point){
    if(!m_scene) return;
    int Order = m_scene->selectedPointOrder(point);
    if(Order >= 0 && Order < 4){
        m_ImgPointLd[Order]->setText(QString("%1,%2").arg(point.x()).arg(point.y()));
        m_textResult->append(QString("已选择第 %1 个点，坐标为：(%2, %3)").arg(Order + 1).arg(point.x()).arg(point.y()));
    }
}

void CameraWarpectiveWidget::onStartCalibClicked(){
    std::vector<cv::Point2f> imgPts, objPts;
    for(int i = 0; i < 4; ++i){
        imgPts.emplace_back(m_ImgPoints[i].x(), m_ImgPoints[i].y());
        objPts.emplace_back(m_ObjPoints[i].x(), m_ObjPoints[i].y());
    }
    float srcW = cv::norm(imgPts[1] - imgPts[0]);  // ≈ 708 像素
    float srcH = cv::norm(imgPts[3] - imgPts[0]);  // ≈ 672 像素
    int outW = (int)srcW;
    int outH = (int)srcH;

    std::vector<cv::Point2f> dstPts = {
    {       0.f,        0.f},
    {outW - 1.f,        0.f},
    {outW - 1.f, outH - 1.f},
    {       0.f, outH - 1.f}
    };
    m_CameraCorrector.setTiltCorrectionByPoints(imgPts,dstPts,cv::Size(outW, outH));
    TiltParams tempParams = m_CameraCorrector.getTiltParams();
    QString resultText = QString("标定完成！\n标定结果: \n%1 , %2 , %3\n%4 , %5 , %6\n%7 , %8 , %9\n输出图像尺寸: %10x%11")
        .arg(tempParams.homography.at<double>(0, 0), 0, 'f', 4) // H[0][0]
        .arg(tempParams.homography.at<double>(0, 1), 0, 'f', 4) // H[0][1]
        .arg(tempParams.homography.at<double>(0, 2), 0, 'f', 4) // H[0][2]
        .arg(tempParams.homography.at<double>(1, 0), 0, 'f', 4) // H[1][0]
        .arg(tempParams.homography.at<double>(1, 1), 0, 'f', 4) // H[1][1]
        .arg(tempParams.homography.at<double>(1, 2), 0, 'f', 4) // H[1][2]
        .arg(tempParams.homography.at<double>(2, 0), 0, 'f', 4) // H[2][0]
        .arg(tempParams.homography.at<double>(2, 1), 0, 'f', 4) // H[2][1]
        .arg(tempParams.homography.at<double>(2, 2), 0, 'f', 4) // H[2][2]
        .arg(tempParams.outputSize.width)
        .arg(tempParams.outputSize.height);
    m_textResult->append(resultText);
    m_CameraCorrector.correctTilt(m_currentImage, m_resultImage);
    onSwitchShowClicked(); // 切换显示结果图

}

void CameraWarpectiveWidget::onSwitchShowClicked(){
    if(!m_scene) return;
    if(m_showOriginal){
        if(m_resultImage.empty()) return;
        cv::Mat rgb;
        cv::cvtColor(m_resultImage, rgb, cv::COLOR_BGR2RGB);
        QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        m_scene->setOriginalPixmap(QPixmap::fromImage(img));
        m_scene->resetImage();
        m_showOriginal = false;
    } else {
        if(m_currentImage.empty()) return;
        cv::Mat rgb;
        cv::cvtColor(m_currentImage, rgb, cv::COLOR_BGR2RGB);
        QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        m_scene->setOriginalPixmap(QPixmap::fromImage(img));
        m_scene->resetImage();
        m_showOriginal = true;
    }
}

