#include "CameraCalibWidget.h"
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

using namespace std;
using namespace cv;

CameraCalibWidget::CameraCalibWidget(QWidget *parent)
    : baseWidget(parent), m_scene(nullptr), m_view(nullptr)
{
    init();
    this->setWindowTitle(QStringLiteral("")); 
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    this->sizePolicy().setHorizontalStretch(0);
    this->sizePolicy().setVerticalStretch(0);
    this->resize(1200, 800);
    cv::Mat m = cv::Mat::zeros(3,3,CV_8UC1);
}

CameraCalibWidget::~CameraCalibWidget()
{
}

void CameraCalibWidget::init()
{
    QGridLayout* contentLayout = new QGridLayout();
    QWidget* LeftWidget = new QWidget(contentLayout->widget());
    QGridLayout* layoutSettings = new QGridLayout(LeftWidget); // 改为网格布局，使左侧变窄

    layoutSettings->addWidget(new QLabel("角点列数:"), 0, 0);
    m_editBoardWidth = new QLineEdit("11"); // 默认值设为 11
    layoutSettings->addWidget(m_editBoardWidth, 0, 1);

    layoutSettings->addWidget(new QLabel("角点行数:"), 1, 0);
    m_editBoardHeight = new QLineEdit("8"); // 默认值设为 8
    layoutSettings->addWidget(m_editBoardHeight, 1, 1);

    // layoutSettings->addWidget(new QLabel("棋盘厚度:"), 2, 0);
    // m_editBoardDepth = new QLineEdit("0.0"); // 默认值设为 0.0
    // m_editBoardDepth->setToolTip("(mm)");
    // layoutSettings->addWidget(m_editBoardDepth, 2, 1);

    layoutSettings->addWidget(new QLabel("方格尺寸:"), 3, 0);
    m_editSquareSize = new QLineEdit("5.0"); // 默认值设为 5.0
    m_editSquareSize->setToolTip("(mm)");
    layoutSettings->addWidget(m_editSquareSize, 3, 1);

    // layoutSettings->addWidget(new QLabel("ArUco尺寸:"), 4, 0);
    // m_editMarkerSize = new QLineEdit("2.5"); // 默认值设为 2.5
    // m_editMarkerSize->setToolTip("(mm) - ChArUco");
    // layoutSettings->addWidget(m_editMarkerSize, 4, 1);

    //layoutSettings->addWidget(new QLabel("字典ID:"), 5, 0);
    m_editDictionaryId = new QLineEdit(""); // 默认为空
    m_editDictionaryId->setPlaceholderText("为空则不使用ChArUco");
    m_editDictionaryId->setToolTip("例如 10 (DICT_6X6_250)");
    //layoutSettings->addWidget(m_editDictionaryId, 5, 1);

    // --- 中间图像显示 ---
    QWidget* imageDisplayWidget = new QWidget(this);
    imageDisplayWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // 显式设置 Expanding

    m_scene = new CameraCalibScene(imageDisplayWidget);
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

    m_listImages = new QListWidget(this);
    m_listImages->setSelectionMode(QAbstractItemView::ExtendedSelection); // 支持多选
    m_listImages->setStyleSheet("QListWidget { background-color: rgb(75, 74, 74); }");
    layoutImages->addWidget(m_listImages);
    connect(m_listImages, &QListWidget::itemSelectionChanged, [this]() {
        QList<QListWidgetItem*> selectedItems = m_listImages->selectedItems();
        if (!selectedItems.isEmpty()) {
            QString firstSelectedPath = selectedItems.first()->text();
            m_currentImage = cv::imread(firstSelectedPath.toLocal8Bit().constData());
            if (m_currentImage.empty()) return;
            cv::Mat rgb;
            cv::cvtColor(m_currentImage, rgb, cv::COLOR_BGR2RGB);
            QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
            m_scene->setOriginalPixmap(QPixmap::fromImage(img));
            m_scene->resetImage();
        }
    });

    QHBoxLayout* layoutImgBtns = new QHBoxLayout();
    m_btnAddImages = new QPushButton("添加图片", this);
    m_btnRemoveImage = new QPushButton("移除选中", this);
    layoutImgBtns->addWidget(m_btnAddImages);
    layoutImgBtns->addWidget(m_btnRemoveImage);
    layoutImages->addLayout(layoutImgBtns);

    QHBoxLayout* btnlayout = new QHBoxLayout();
    m_btnCalibrate = new QPushButton("开始标定", this);
    btnlayout->addWidget(m_btnCalibrate);

    // 保存按钮
    m_btnSave = new QPushButton("保存标定结果", this);
    m_btnSave->setEnabled(false); // 初始禁用，标定成功后启用
    btnlayout->addWidget(m_btnSave);
    layoutImages->addLayout(btnlayout);

    layoutSettings->addLayout(layoutImages, 4, 0, 1, 2);

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

    
    connect(m_btnAddImages, &QPushButton::clicked, this, &CameraCalibWidget::onAddImagesClicked);
    connect(m_btnRemoveImage, &QPushButton::clicked, this, &CameraCalibWidget::onRemoveImageClicked);
    connect(m_btnCalibrate, &QPushButton::clicked, this, &CameraCalibWidget::onCalibrateClicked);
    connect(m_btnSave, &QPushButton::clicked, this, &CameraCalibWidget::onSaveClicked);

}

void CameraCalibWidget::onAddImagesClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        "选择标定图片",
        "", // 默认路径
        "Images (*.png *.jpg *.bmp *.jpeg)" // 文件过滤器
    );

    if (!files.isEmpty()) {
        m_listImages->addItems(files);
    }
    imagePaths.clear();
    for (int i = 0; i < m_listImages->count(); ++i) {
        imagePaths.push_back(m_listImages->item(i)->text().toLocal8Bit().constData());
    }
    m_currentImage = cv::imread(files[0].toLocal8Bit().constData());
    if (m_currentImage.empty()) return;
    cv::Mat rgb;
    cv::cvtColor(m_currentImage, rgb, cv::COLOR_BGR2RGB);
    QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    m_scene->setOriginalPixmap(QPixmap::fromImage(img));
    m_scene->resetImage();
}

void CameraCalibWidget::onRemoveImageClicked()
{
    QList<QListWidgetItem*> items = m_listImages->selectedItems();
    for (auto item : items) {
        delete m_listImages->takeItem(m_listImages->row(item));
    }
    imagePaths.clear();
    for (int i = 0; i < m_listImages->count(); ++i) {
        imagePaths.push_back(m_listImages->item(i)->text().toLocal8Bit().constData());
    }
}

void CameraCalibWidget::onCalibrateClicked()
{
    bool ok;
    int Width = m_editBoardWidth->text().toInt(&ok);
    if (!ok || Width <= 0) {
        m_textResult->append("请输入有效的角点列数");
        return;
    }
    
    int Height = m_editBoardHeight->text().toInt(&ok);
    if (!ok || Height <= 0) {
        m_textResult->append("请输入有效的角点行数");
        return;
    }

    float size = m_editSquareSize->text().toFloat(&ok);
    if (!ok || size <= 0) {
        m_textResult->append("请输入有效的方格尺寸");
        return;
    }

    if (imagePaths.empty()) {
        m_textResult->append("请先添加标定图片");
        return;
    }

    cv::Mat cameraMatrix, distCoeffs;
    std::vector<cv::Mat> rvecs, tvecs;
    std::vector<std::vector<cv::Point2f>> imagePoints;
    std::vector<std::vector<cv::Point3f>> objectPoints;
    cv::Size boardSize(Width, Height);

    m_textResult->append("正在标定中，请稍候...");
    // 强制刷新界面，显示提示信息，防止界面卡死
    QApplication::processEvents(); 

    // 判断是否使用 ChArUco
    bool useCharuco = false;
    int dictId = -1;
    float markerSize = 0.0f;
    if (!m_editDictionaryId->text().trimmed().isEmpty()) {
        dictId = m_editDictionaryId->text().toInt(&ok);
        markerSize = m_editMarkerSize->text().toFloat();
        if (ok && dictId >= 0 && markerSize > 0) {
            useCharuco = true;
        }
    }

    double rms = -1.0;
    if (useCharuco) {
        m_textResult->append("正在使用 ChArUco 方式进行标定 (DictID: " + QString::number(dictId) + ")...");
        rms = m_calib.runCalibrationChArUco(imagePaths, boardSize, size, markerSize, dictId, 
                                            cameraMatrix, distCoeffs, rvecs, tvecs, imagePoints, objectPoints);
    } else {
        m_textResult->append("正在使用普通棋盘格方式进行标定...");
        rms = m_calib.runCalibration(imagePaths, boardSize, size, cameraMatrix, distCoeffs, rvecs, tvecs, imagePoints, objectPoints);
    }

    if (rms >= 0) {
        // 保存结果到成员变量
        m_isCalibrated = true;
        m_cameraMatrix = cameraMatrix;
        m_distCoeffs = distCoeffs;
        m_rvecs = rvecs;
        m_tvecs = tvecs;
        m_rms = rms;
        m_imagePoints = imagePoints;
        m_objectPoints = objectPoints;
        
        // 启用保存按钮
        m_btnSave->setEnabled(true);

        QString resultStr;
        resultStr += QString("标定成功！\n");
        resultStr += QString("重投影误差 (RMS Error): %1\n\n").arg(rms);
        
        resultStr += "相机内参矩阵 (Camera Matrix):\n";
        // 格式化输出内参矩阵
        resultStr += QString("[%1, %2, %3]\n").arg(cameraMatrix.at<double>(0,0)).arg(cameraMatrix.at<double>(0,1)).arg(cameraMatrix.at<double>(0,2));
        resultStr += QString("[%1, %2, %3]\n").arg(cameraMatrix.at<double>(1,0)).arg(cameraMatrix.at<double>(1,1)).arg(cameraMatrix.at<double>(1,2));
        resultStr += QString("[%1, %2, %3]\n\n").arg(cameraMatrix.at<double>(2,0)).arg(cameraMatrix.at<double>(2,1)).arg(cameraMatrix.at<double>(2,2));

        resultStr += "畸变系数 (Distortion Coefficients):\n";
        // 畸变系数(k1, k2, p1, p2, k3)
        for (int i = 0; i < distCoeffs.cols; ++i) {
             resultStr += QString("%1 ").arg(distCoeffs.at<double>(0, i));
        }
        resultStr += "\n";

        m_textResult->setText(resultStr);
        m_textResult->append("标定完成！请点击“保存标定结果”按钮保存数据。");
    } else {
        m_isCalibrated = false;
        m_btnSave->setEnabled(false);
        m_textResult->append("标定失败！可能是图片中未检测到足够的角点。");
        m_textResult->append("标定失败，请检查图片和参数设置。");
    }
}

void CameraCalibWidget::onSaveClicked()
{
    if (!m_isCalibrated) return;

    QString savePath = QFileDialog::getSaveFileName(this, "保存标定结果", "calibration_result.xml", "OpenCV XML (*.xml);;OpenCV YAML (*.yaml);;Text File (*.txt)");
    if (savePath.isEmpty()) return;

    // 根据后缀名判断保存格式
    if (savePath.endsWith(".xml") || savePath.endsWith(".yaml") || savePath.endsWith(".yml")) {
        try {
            cv::FileStorage fs(savePath.toStdString(), cv::FileStorage::WRITE);
            
            // 1. 标定时间
            fs << "calibration_time" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
            
            // 2. 图像尺寸
            fs << "image_width" << m_imageSize.width;
            fs << "image_height" << m_imageSize.height;
            
            // 3. 相机内参矩阵
            fs << "camera_matrix" << m_cameraMatrix;
            
            // 4. 畸变系数
            fs << "distortion_coefficients" << m_distCoeffs;
            
            // 5. 重投影误差
            fs << "rms_error" << m_rms;
            
            // 6. 外参数 (每张图片的旋转和平移)
            fs << "rvecs" << m_rvecs;
            fs << "tvecs" << m_tvecs;

            // 7. 可选：计算并保存优化后的新相机矩阵 (New Camera Matrix)
            // alpha=1 保留所有像素，alpha=0 裁剪掉无效像素
            cv::Mat newCameraMatrix = cv::getOptimalNewCameraMatrix(m_cameraMatrix, m_distCoeffs, m_imageSize, 1, m_imageSize, 0);
            fs << "new_camera_matrix" << newCameraMatrix;

            fs.release();
            QMessageBox::information(this, "保存成功", "标定结果已保存。");
            isSaved = true;
        } catch (const cv::Exception& e) {
            QMessageBox::critical(this, "保存失败", QString("OpenCV 异常: %1").arg(e.what()));
            isSaved = false;
        }
    } else {
        // 保存为文本格式
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "Calibration Result\n";
            out << "==================\n";
            out << "Date: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
            out << "RMS Error: " << m_rms << "\n";
            out << "Image Size: " << m_imageSize.width << " x " << m_imageSize.height << "\n\n";
            
            out << "Camera Matrix (K):\n";
            out << m_cameraMatrix.at<double>(0,0) << ", " << m_cameraMatrix.at<double>(0,1) << ", " << m_cameraMatrix.at<double>(0,2) << "\n";
            out << m_cameraMatrix.at<double>(1,0) << ", " << m_cameraMatrix.at<double>(1,1) << ", " << m_cameraMatrix.at<double>(1,2) << "\n";
            out << m_cameraMatrix.at<double>(2,0) << ", " << m_cameraMatrix.at<double>(2,1) << ", " << m_cameraMatrix.at<double>(2,2) << "\n\n";
            
            out << "Distortion Coefficients (k1, k2, p1, p2, k3...):\n";
            for (int i = 0; i < m_distCoeffs.cols; ++i) {
                out << m_distCoeffs.at<double>(0, i) << (i < m_distCoeffs.cols - 1 ? ", " : "");
            }
            out << "\n\n";

            out << "Extrinsics (Per Image):\n";
            for(size_t i=0; i<m_rvecs.size(); ++i) {
                out << "Image " << i+1 << ":\n";
                out << "  Rvec: " << m_rvecs[i].at<double>(0) << ", " << m_rvecs[i].at<double>(1) << ", " << m_rvecs[i].at<double>(2) << "\n";
                out << "  Tvec: " << m_tvecs[i].at<double>(0) << ", " << m_tvecs[i].at<double>(1) << ", " << m_tvecs[i].at<double>(2) << "\n";
            }
            out << "\n";

            out << "Detected Points Details\n";
            out << "=======================\n";
            for (size_t i = 0; i < m_imagePoints.size(); ++i) {
                out << "Image " << i + 1 << ":\n";
                out << "  Object Points (World Coords) | Image Points (Pixel Coords)\n";
                for (size_t j = 0; j < m_imagePoints[i].size(); ++j) {
                    const auto& objPt = m_objectPoints[i][j];
                    const auto& imgPt = m_imagePoints[i][j];
                    out << "  (" << objPt.x << ", " << objPt.y << ", " << objPt.z << ") | "
                        << "(" << imgPt.x << ", " << imgPt.y << ")\n";
                }
                out << "\n";
            }
            
            file.close();
            m_textResult->append("文本格式标定结果已保存。");
            isSaved = true;
        } else {
            m_textResult->append("无法写入文件。");
            isSaved = false;
        }
    }
}
