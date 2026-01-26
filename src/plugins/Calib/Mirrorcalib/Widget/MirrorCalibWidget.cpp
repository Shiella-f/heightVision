#include "MirrorCalibWidget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QDebug>
#include <fstream>
#include <sstream>
#include <QFileInfo>
#include <QStatusBar>

MirrorCalibWidget::MirrorCalibWidget(QWidget *parent) : baseWidget(parent), m_core(new MirrorCalib) 
{
    init();
    this->setWindowTitle(QStringLiteral("")); 
        // 设置无边框窗口
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);

    this->sizePolicy().setHorizontalStretch(0);
    this->sizePolicy().setVerticalStretch(0);
    this->resize(1200, 800);

}

MirrorCalibWidget::~MirrorCalibWidget() {
    delete m_core;
}

void MirrorCalibWidget::init()
{

    auto newButton = [](QToolButton* btn, const QString& text) {
        btn->setText(text);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        btn->setFixedSize(100, 20);
        return btn;
    };
    auto newLabel = [](QLabel* label, const QString& text) {
        label->setText(text);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return label;
    };

    auto newHorizontalLayout = [](QHBoxLayout* layout) {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        return layout;
    };

    QVBoxLayout* controlLayout = new QVBoxLayout();
    controlLayout->setContentsMargins(5, 0, 0, 0);
    QWidget* controlWidget = new QWidget(this);
    controlWidget->setContentsMargins(0, 0, 0, 0);
    controlWidget->setLayout(controlLayout);
    //QGroupBox* snapGroup = new QGroupBox("1. 图像采集", this);
    QHBoxLayout* snapLayout = new QHBoxLayout();
    snapLayout->setContentsMargins(0, 0, 0, 0);
    m_Snap3x3btn = newButton(new QToolButton(this), QStringLiteral("3x3采集图像"));
    connect(m_Snap3x3btn, &QToolButton::clicked, this, &MirrorCalibWidget::onSnap3x3Image);
    snapLayout->addWidget(m_Snap3x3btn);

    m_Snap9x9btn = newButton(new QToolButton(this), QStringLiteral("9x9采集图像"));
    connect(m_Snap9x9btn, &QToolButton::clicked, this, &MirrorCalibWidget::onSnap9x9Image);
    snapLayout->addWidget(m_Snap9x9btn);
    controlLayout->addLayout(snapLayout);

    //QGroupBox* calibGroup = new QGroupBox("2. 振镜校正操作", this);
    QGridLayout* calibLayout = new QGridLayout();
    calibLayout->setContentsMargins(0, 0, 0, 0);
    m_Calib3x3btn = newButton(new QToolButton(this), QStringLiteral("3x3振镜校正"));
    connect(m_Calib3x3btn, &QToolButton::clicked, this, &MirrorCalibWidget::onCalibrate3x3);
    calibLayout->addWidget(m_Calib3x3btn, 0, 0);
    
    m_Calib9x9btn = newButton(new QToolButton(this), QStringLiteral("9x9振镜校正"));
    connect(m_Calib9x9btn, &QToolButton::clicked, this, &MirrorCalibWidget::onCalibrate9x9);
    calibLayout->addWidget(m_Calib9x9btn, 0, 1);

    m_setRoibox = new QCheckBox(QStringLiteral("设置ROI"), this);
    m_setRoibox->setChecked(false);

    connect(m_setRoibox, &QCheckBox::stateChanged, this, [this](int state){
        bool enabled = (state == Qt::Checked);
        m_MirrorScene->setRoiSelectionEnabled(enabled);
    });

    calibLayout->addWidget(m_setRoibox, 1, 0);

    m_clearRoibtn = newButton(new QToolButton(this), QStringLiteral("清除ROI"));
    connect(m_clearRoibtn, &QToolButton::clicked, this, [this](){
        m_MirrorScene->clearRoi();
        if(m_setRoibox->isChecked()) {
            m_MirrorScene->setRoiSelectionEnabled(true);
        }
    });
    calibLayout->addWidget(m_clearRoibtn, 1, 1);

    m_ConfirmSizebtn = newButton(new QToolButton(this), QStringLiteral("保存成像数据"));
    qInfo() << m_ConfirmSizebtn->size();
    connect(m_ConfirmSizebtn, &QToolButton::clicked, this, &MirrorCalibWidget::onConfirmSize);
    calibLayout->addWidget(m_ConfirmSizebtn, 2, 0);
    m_savedatebtn = newButton(new QToolButton(this), QStringLiteral("保存校正数据"));
    connect(m_savedatebtn, &QToolButton::clicked, this, &MirrorCalibWidget::onSaveCalibData);
    calibLayout->addWidget(m_savedatebtn, 2, 1);
    

    //calibLayout->addLayout(PtsLayout, 2, 0, 1, 2);
    // 添加分组框到左侧布局
    controlLayout->addLayout(calibLayout);
    controlLayout->addStretch();

    // --- 中间图像显示 ---
    m_MirrorScene = new MirrorScene(this);
    m_view = new QGraphicsView(this);
    m_view->setMouseTracking(true); // 启用鼠标跟踪
    m_view->setInteractive(true);
    m_MirrorScene->attachView(m_view); // 让场景附加到视图，启用缩放等行为
    m_view->setStyleSheet("background-color: rgb(40, 40, 40);border:1px solid gray;");
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);




    QVBoxLayout* rightLayout = new QVBoxLayout();
    m_resultTable = new QTableWidget(this);
    m_resultTable->setColumnCount(5);
    for (int col = 0; col < m_resultTable->columnCount(); ++col) {
        m_resultTable->setColumnWidth(col, 70);
    }
      for (int row = 0; row < m_resultTable->rowCount(); ++row) {
        m_resultTable->setRowHeight(row, 30);
    }
    m_resultTable->setHorizontalHeaderLabels({"Index", "Pre X", "Pre Y", "Det X", "Det Y"});
    rightLayout->addWidget(m_resultTable);

    QStatusBar* statusBar = new QStatusBar(this);
    statusBar->setStyleSheet("QStatusBar { background-color:#4e4e4e; border-top: 1px solid gray; color: white; } QLabel { color: white; }");
    statusBar->setMaximumSize(QSize(QWIDGETSIZE_MAX, 25));

    QWidget* statusWidget = new QWidget(this);
    QHBoxLayout* PtsLayout = new QHBoxLayout(statusWidget);
    PtsLayout->setContentsMargins(5, 0, 5, 0);
    statusWidget->setWindowFlags(Qt::FramelessWindowHint);

    QLabel* PtsLabel = new QLabel(QStringLiteral("坐标("), statusWidget);
    QLabel* PtsValueLabel = new QLabel(QStringLiteral("  ,  "), statusWidget);
    QLabel* PtsUnitLabel = new QLabel(QStringLiteral(")"), statusWidget);

    PtsLayout->addWidget(PtsLabel);
    PtsLayout->addWidget(PtsValueLabel);
    PtsLayout->addWidget(PtsUnitLabel);
    
    statusBar->addWidget(statusWidget);

    connect(m_MirrorScene, &MirrorScene::currentPointChanged, this, [PtsValueLabel](const QPointF& pt){
        PtsValueLabel->setText(QString::number(pt.x()) + " , " + QString::number(pt.y()));
    });

    QHBoxLayout* contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(1, 1, 1, 1);
    contentLayout->addWidget(controlWidget, 1);
    contentLayout->addWidget(m_view, 3);
    contentLayout->addLayout(rightLayout, 2);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 5, 0);
    mainLayout->setSpacing(0);
    mainLayout->addLayout(contentLayout);
    mainLayout->addWidget(statusBar); // 状态栏固定在底部
    
    QVBoxLayout* finalLayout = new QVBoxLayout(this);
    finalLayout->addLayout(mainLayout);
}

void MirrorCalibWidget::logMessage(const QString& msg) {
    qDebug() << msg;
}

cv::Mat MirrorCalibWidget::getCurrentCameraImage() {
    QString fileName = QFileDialog::getOpenFileName(this, "选择图像", "", "Images (*.png *.jpg *.bmp)");
    if (fileName.isEmpty()) return cv::Mat();
    std::string fileStr = fileName.toLocal8Bit().constData();
    return cv::imread(fileStr);
}

void MirrorCalibWidget::onSnap3x3Image() {

    m_currentImage = getCurrentCameraImage();
    
    if (!m_currentImage.empty()) {
        m_MirrorScene->clearOverlays();
        cv::Mat rgb;
        cv::cvtColor(m_currentImage, rgb, cv::COLOR_BGR2RGB);
        QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        m_MirrorScene->setOriginalPixmap(QPixmap::fromImage(img));
        m_MirrorScene->resetImage();

        logMessage("Snap is success (3x3 Marker)");
    } else {
        logMessage("Snap failed");
    }
}

void MirrorCalibWidget::onSnap9x9Image() {
    m_currentImage = getCurrentCameraImage();
    if (!m_currentImage.empty()) {
        m_MirrorScene->clearOverlays();
        cv::Mat rgb;
        cv::cvtColor(m_currentImage, rgb, cv::COLOR_BGR2RGB);
        QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        m_MirrorScene->setOriginalPixmap(QPixmap::fromImage(img));
        m_MirrorScene->resetImage();
        logMessage("Snap is success (9x9 Marker)");
    } else {
        logMessage("Snap failed");
    }
}

void MirrorCalibWidget::onCalibrate3x3() {
    m_roiArea = m_MirrorScene->getRoiArea();
    if (m_currentImage.empty()) {
        logMessage("请先采集3x3图像");
        return;
    }
    cv::Mat workImg;
    workImg.create(m_currentImage.size(), m_currentImage.type());
    if(m_roiArea.width() > 0 && m_roiArea.height() > 0) {
        cv::Rect roiRect(static_cast<int>(m_roiArea.left()), static_cast<int>(m_roiArea.top()),
                         static_cast<int>(m_roiArea.width()), static_cast<int>(m_roiArea.height()));
        // 确保ROI在图像范围内
        roiRect &= cv::Rect(0, 0, m_currentImage.cols, m_currentImage.rows);
        m_currentImage(roiRect).copyTo(workImg(roiRect));
    }else {
        workImg = m_currentImage;
    }
    cv::Size patternSize(3, 3);
    
    bool found = m_core->findMarkerPoints(workImg, patternSize, m_3x3Points);
    
    if (found) {
        // 如果找到
        m_MirrorScene->clearOverlays();
        if(m_3x3Points.size() != 9) {
            logMessage("检测到的3x3标记点数量不正确");
            return;
        }
        std::sort(m_3x3Points.begin(), m_3x3Points.end(), [](const cv::Point2f& a, const cv::Point2f& b) {
            return (a.y < b.y);
        });
        for(int row = 0; row < 3; ++row) {
            auto rowStart = m_3x3Points.begin() + row * 3;
            auto rowEnd = rowStart + 3;

            std::sort(rowStart, rowEnd, [](const cv::Point2f& a, const cv::Point2f& b) {
                return a.x < b.x;
            });
        }
        int pointsize = (workImg.cols)/900 *5;
        for (const auto& pt : m_3x3Points) {
            m_MirrorScene->addOverlayPoint(QPointF(pt.x, pt.y), QPen(Qt::red), pointsize);
        }

        std::vector<cv::Point2f> preset3_3Points;
        float spacing = 60.0f;
        int width = m_currentImage.cols;
        int height = m_currentImage.rows;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                float physX = j * spacing;
                float physY = i * spacing;
                preset3_3Points.push_back(cv::Point2f(physX, physY));
            }
        }
        
        m_resultTable->setRowCount(m_3x3Points.size());
        for (size_t i = 0; i < m_3x3Points.size(); ++i) {
            m_resultTable->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
            if (i < preset3_3Points.size()) {
                m_resultTable->setItem(i, 1, new QTableWidgetItem(QString::number(preset3_3Points[i].x, 'f', 2)));
                m_resultTable->setItem(i, 2, new QTableWidgetItem(QString::number(preset3_3Points[i].y, 'f', 2)));
            }
            m_resultTable->setItem(i, 3, new QTableWidgetItem(QString::number(m_3x3Points[i].x, 'f', 2)));
            m_resultTable->setItem(i, 4, new QTableWidgetItem(QString::number(m_3x3Points[i].y, 'f', 2)));
        }
    } else {
        QMessageBox::warning(this, "标定失败", "未检测到3x3标记点");
        logMessage("未检测到3x3标记点");
    }


        
}

void MirrorCalibWidget::onCalibrate9x9() {
    m_roiArea = m_MirrorScene->getRoiArea();
    if (m_currentImage.empty()) {
        logMessage("请先采集9x9图像");
        return;
    }
    cv::Mat workImg;
    workImg.create(m_currentImage.size(), m_currentImage.type());
    if(m_roiArea.width() > 0 && m_roiArea.height() > 0) {
        cv::Rect roiRect(static_cast<int>(m_roiArea.left()), static_cast<int>(m_roiArea.top()),
                         static_cast<int>(m_roiArea.width()), static_cast<int>(m_roiArea.height()));
        // 确保ROI在图像范围内
        roiRect &= cv::Rect(0, 0, m_currentImage.cols, m_currentImage.rows);
        m_currentImage(roiRect).copyTo(workImg(roiRect));
    }else {
        workImg = m_currentImage;
    }
    cv::Size patternSize(9, 9);
    bool found = m_core->findMarkerPoints(workImg, patternSize, m_9x9Points);
    
    if (found) {
        m_MirrorScene->clearOverlays();
        if(m_9x9Points.size() != 81) {
            logMessage("检测到的9x9标记点数量不正确");
            return;
        }
        std::sort(m_9x9Points.begin(), m_9x9Points.end(), [](const cv::Point2f& a, const cv::Point2f& b) {
            return (a.y < b.y);
        });
        for(int row = 0; row < 9; ++row) {
            auto rowStart = m_9x9Points.begin() + row * 9;
            auto rowEnd = rowStart + 9;

            std::sort(rowStart, rowEnd, [](const cv::Point2f& a, const cv::Point2f& b) {
                return a.x < b.x;
            });
        }
        int pointsize = (workImg.cols)/900 *3;
        for (const auto& pt : m_9x9Points) {
            m_MirrorScene->addOverlayPoint(QPointF(pt.x, pt.y), QPen(Qt::blue), pointsize);
        }
        
        std::vector<cv::Point2f> preset9_9Points;
        float spacing = 15.0f;
        int width = m_currentImage.cols;
        int height = m_currentImage.rows;
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 9; j++) {
                float physX = j * spacing;
                float physY = i * spacing;
                preset9_9Points.push_back(cv::Point2f(physX, physY));
            }
        }
        
        m_resultTable->setRowCount(m_9x9Points.size());
        for (size_t i = 0; i < m_9x9Points.size(); ++i) {
            m_resultTable->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
            if (i < preset9_9Points.size()) {
                m_resultTable->setItem(i, 1, new QTableWidgetItem(QString::number(preset9_9Points[i].x, 'f', 2)));
                m_resultTable->setItem(i, 2, new QTableWidgetItem(QString::number(preset9_9Points[i].y, 'f', 2)));
            }
            m_resultTable->setItem(i, 3, new QTableWidgetItem(QString::number(m_9x9Points[i].x, 'f', 2)));
            m_resultTable->setItem(i, 4, new QTableWidgetItem(QString::number(m_9x9Points[i].y, 'f', 2)));
        }
        
        cv::Mat homographyMatrix;
        double error = m_core->calibrateHomography(m_9x9Points, preset9_9Points, homographyMatrix);
        if (error >= 0) {
            logMessage(QString("标定成功! 平均误差: %1 mm").arg(error));
        }
    }
}
        
void MirrorCalibWidget::onConfirmSize() 
{
    if (m_currentImage.empty()) {
        logMessage("请先采集或加载一张图像"); 
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(this, "选择3x3标定结果文件", "", "Calibration Files (*.xml *.csv)"); 
    if (filePath.isEmpty()) return; 

    cv::Mat homographyMatrix;

    if (filePath.endsWith(".xml", Qt::CaseInsensitive)) 
    {
        cv::FileStorage fs(filePath.toStdString(), cv::FileStorage::READ);
        if (fs.isOpened()) {
            fs["HomographyMatrix"] >> homographyMatrix;
            fs.release(); 
        }
    } else if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
 
        std::vector<cv::Point2f> detected, preset;
        std::ifstream file(filePath.toStdString());
        if (file.is_open()) {
            std::string line;
            std::getline(file, line);
            while (std::getline(file, line)) {
                std::stringstream ss(line);
                std::string segment;
                std::vector<std::string> seglist;
                while (std::getline(ss, segment, ',')) {
                    seglist.push_back(segment);
                }
                if (seglist.size() >= 5) {
                    preset.push_back(cv::Point2f(std::stof(seglist[1]), std::stof(seglist[2])));
                    detected.push_back(cv::Point2f(std::stof(seglist[3]), std::stof(seglist[4])));
                }
            }
            file.close();
            if (!detected.empty() && !preset.empty()) {
                m_core->calibrateHomography(detected, preset, homographyMatrix);
            }
        }
    }

    if (homographyMatrix.empty()) {
        logMessage("无法获取有效的标定矩阵");
        return;
    }

    // 3x3标定点物理坐标范围是 0~120 (中心 60,60)
    // 目标截取 130x130mm，以 (60,60) 为中心
    // 物理坐标范围: X[-5, 125], Y[-5, 125]
    std::vector<cv::Point2f> physCorners;
    physCorners.push_back(cv::Point2f(-5, -5));
    physCorners.push_back(cv::Point2f(125, -5));
    physCorners.push_back(cv::Point2f(125, 125));
    physCorners.push_back(cv::Point2f(-5, 125));

    // 计算逆矩阵: 物理 -> 像素
    cv::Mat invH = homographyMatrix.inv();
    std::vector<cv::Point2f> imgCorners;
    cv::perspectiveTransform(physCorners, imgCorners, invH);

    logMessage("=== 130x130mm 区域像素顶点坐标 ===");
    const char* cornerNames[] = {"左上(-5,-5)", "右上(125,-5)", "右下(125,125)", "左下(-5,125)"};
    for(size_t i=0; i<imgCorners.size(); ++i) {
        logMessage(QString("%1: (%2, %3)").arg(cornerNames[i]).arg(imgCorners[i].x, 0, 'f', 2).arg(imgCorners[i].y, 0, 'f', 2));
    }

    // 获取像素包围盒
    cv::Rect roi = cv::boundingRect(imgCorners);
    logMessage(QString("裁剪包围盒(ROI): x=%1, y=%2, w=%3, h=%4").arg(roi.x).arg(roi.y).arg(roi.width).arg(roi.height));

    // 限制在图像范围内
    roi = roi & cv::Rect(0, 0, m_currentImage.cols, m_currentImage.rows);

    if (roi.area() > 0) {
        cv::Mat cropped = m_currentImage(roi).clone();
        QString savePath = QFileDialog::getSaveFileName(this, "保存130x130mm裁剪图像", "cropped_130x130.bmp", "Images (*.bmp *.png *.jpg)");
        if (!savePath.isEmpty()) {
            cv::imwrite(savePath.toStdString(), cropped);
            logMessage("已保存裁剪图像至: " + savePath);

            // 保存ROI信息到同名txt文件
            QFileInfo fileInfo(savePath);
            QString roiPath = fileInfo.absolutePath() + "/" + fileInfo.baseName() + "_roi.txt";
            std::ofstream roiFile(roiPath.toStdString());
            if (roiFile.is_open()) {
                roiFile << "X=" << roi.x << "\n";
                roiFile << "Y=" << roi.y << "\n";
                roiFile << "Width=" << roi.width << "\n";
                roiFile << "Height=" << roi.height << "\n";
                
                // 同时也保存四个顶点坐标，方便参考
                roiFile << "--------------------------------\n";
                roiFile << "TopLeft_X=" << imgCorners[0].x << "\n";
                roiFile << "TopLeft_Y=" << imgCorners[0].y << "\n";
                roiFile << "TopRight_X=" << imgCorners[1].x << "\n";
                roiFile << "TopRight_Y=" << imgCorners[1].y << "\n";
                roiFile << "BottomRight_X=" << imgCorners[2].x << "\n";
                roiFile << "BottomRight_Y=" << imgCorners[2].y << "\n";
                roiFile << "BottomLeft_X=" << imgCorners[3].x << "\n";
                roiFile << "BottomLeft_Y=" << imgCorners[3].y << "\n";

                roiFile.close();
                logMessage("ROI及顶点信息已保存至: " + roiPath);
            }
        }
    } else {
        logMessage("裁剪区域无效（可能超出图像范围）");
    }
}

void MirrorCalibWidget::onSaveCalibData()
{
    int rows = m_resultTable->rowCount();
    if (rows <= 0) {
        logMessage("没有数据可保存");
        return;
    }

    std::vector<cv::Point2f> presetPoints;
    std::vector<cv::Point2f> detectedPoints;

    for (int i = 0; i < rows; ++i) {
        QTableWidgetItem* itemPx = m_resultTable->item(i, 1);
        QTableWidgetItem* itemPy = m_resultTable->item(i, 2);
        QTableWidgetItem* itemDx = m_resultTable->item(i, 3);
        QTableWidgetItem* itemDy = m_resultTable->item(i, 4);

        if (itemPx && itemPy && itemDx && itemDy) {
            float preX = itemPx->text().toFloat();
            float preY = itemPy->text().toFloat();
            float detX = itemDx->text().toFloat();
            float detY = itemDy->text().toFloat();
            
            presetPoints.push_back(cv::Point2f(preX, preY));
            detectedPoints.push_back(cv::Point2f(detX, detY));
        }
    }

    if (presetPoints.size() != detectedPoints.size() || presetPoints.empty()) return;

    // 区分 3x3 (9点) 和 9x9 (81点)
    if (rows == 9) {
        QString csvFile = QFileDialog::getSaveFileName(this, "保存3x3对比数据", "33.CSV", "CSV Files (*.csv)");
        if (!csvFile.isEmpty()) {
            m_core->saveComparisonToCSV(csvFile, detectedPoints, presetPoints);
            logMessage("3x3 校正数据已保存至 " + csvFile);
        }
    } else if (rows == 81) {
        QString csvFile = QFileDialog::getSaveFileName(this, "保存9x9对比数据", "99.CSV", "CSV Files (*.csv)");
        if (!csvFile.isEmpty()) {
            m_core->saveComparisonToCSV(csvFile, detectedPoints, presetPoints);
            logMessage("9x9 校正数据已保存至 " + csvFile);
        }

        // 保存 Matrix
        cv::Mat homographyMatrix;
        double error = m_core->calibrateHomography(detectedPoints, presetPoints, homographyMatrix);
        
        if (error >= 0) {
             QString xmlFile = QFileDialog::getSaveFileName(this, "保存标定结果矩阵", "calibration9_9_result.xml", "XML Files (*.xml)");
             if (!xmlFile.isEmpty()) {
                 m_core->saveCalibrationMatrix(xmlFile, homographyMatrix);
                 logMessage("标定矩阵已保存至 " + xmlFile);
             }
        } else {
            logMessage("数据不足或误差过大，无法生成标定矩阵");
        }
    } else {
        QString csvFile = QFileDialog::getSaveFileName(this, "保存当前数据", "calib_data.CSV", "CSV Files (*.csv)");
        if (!csvFile.isEmpty()) {
            m_core->saveComparisonToCSV(csvFile, detectedPoints, presetPoints);
            logMessage("当前列表数据已保存至 " + csvFile);
        }
    }
}

