#include "MirrorCalibFunction.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QDebug>
#include <fstream>
#include <sstream>
#include <QFileInfo>
#include <QStatusBar>
#include <QLineEdit>
#include <QWidget>
MirrorCalibFunction::MirrorCalibFunction(QWidget *parent) : m_core(new MirrorCalib) 
{
    m_MirrorScene = new MirrorScene(this);
    m_resultTable = new QTableWidget(nullptr);
}

MirrorCalibFunction::~MirrorCalibFunction() {
    delete m_core;
    delete corrector;
}


cv::Mat MirrorCalibFunction::getCurrentCameraImage() {
    QString fileName = QFileDialog::getOpenFileName(nullptr, "选择图像", "", "Images (*.png *.jpg *.bmp)");
    if (fileName.isEmpty()) return cv::Mat();
    std::string fileStr = fileName.toLocal8Bit().constData();
    return cv::imread(fileStr);
}
        
void MirrorCalibFunction::onConfirmSize() 
{
    if (m_currentImage.empty()) {
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(nullptr, "选择3x3标定结果文件", "", "Calibration Files (*.xml *.csv)"); 
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

    const char* cornerNames[] = {"左上(-5,-5)", "右上(125,-5)", "右下(125,125)", "左下(-5,125)"};
    for(size_t i=0; i<imgCorners.size(); ++i) {
    }

    // 获取像素包围盒
    cv::Rect roi = cv::boundingRect(imgCorners);

    // 限制在图像范围内
    roi = roi & cv::Rect(0, 0, m_currentImage.cols, m_currentImage.rows);

    if (roi.area() > 0) {
        cv::Mat cropped = m_currentImage(roi).clone();
        QString savePath = QFileDialog::getSaveFileName(nullptr, "保存130x130mm裁剪图像", "cropped_130x130.bmp", "Images (*.bmp *.png *.jpg)");
        if (!savePath.isEmpty()) {
            cv::imwrite(savePath.toStdString(), cropped);

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
            }
        }
    }
}

bool MirrorCalibFunction::Calibrate3x3(const cv::Mat& image){
    cv::Size patternSize(3, 3);
    cv::Mat workImg = image.clone();
    bool found = m_core->findMarkerPoints(workImg, patternSize, m_3x3Points);
    
    if (found) {
        // 如果找到
        m_MirrorScene->clearOverlays();
        if(m_3x3Points.size() != 9) {
            return false;
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
    }
    return found;  
}

bool MirrorCalibFunction::Calibrate9x9(const cv::Mat& image){
    if (image.empty()) {
        return false;
    }
    if (!m_core || !m_MirrorScene || !m_resultTable) {
        return false;
    }
    if (!corrector) {
        corrector = new CameraCorrector();
    }

    m_currentImage = image;
    cv::Size patternSize(9, 9);
    cv::Mat workImg = image.clone();
    bool found = m_core->findMarkerPoints(workImg, patternSize, m_9x9Points);
    std::vector<cv::Point2f> new_9x9Points;
    if (found) {
        m_MirrorScene->clearOverlays();
        if(m_9x9Points.size() != 81) {
            return false;
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
        if(m_Canvassize.x >= 0 || m_Canvassize.y >= 0) {
            cv::Point2f srccenter = m_9x9Points[(m_9x9Points.size() -1) / 2]; // 9x9中心点索引
            cv::Point2f dstcenter = cv::Point2f(m_Canvassize.x/2, m_Canvassize.y/2); // 物理坐标中心
            corrector->setCoordinateOriginByPair(srccenter,dstcenter); // 根据实际情况调整坐标偏移
        }
        
        std::vector<cv::Point2f> new_9x9Points;
        for(auto &pt : m_9x9Points) {
            cv::Point2f correctedPt = corrector->pixelToMm(pt);
            new_9x9Points.push_back(correctedPt);
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
        m_resultTable->setRowCount(new_9x9Points.size());
        for (size_t i = 0; i < new_9x9Points.size(); ++i) {
            m_resultTable->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
            if (i < preset9_9Points.size()) {
                m_resultTable->setItem(i, 1, new QTableWidgetItem(QString::number(preset9_9Points[i].x, 'f', 2)));
                m_resultTable->setItem(i, 2, new QTableWidgetItem(QString::number(preset9_9Points[i].y, 'f', 2)));
            }
            m_resultTable->setItem(i, 3, new QTableWidgetItem(QString::number(new_9x9Points[i].x, 'f', 2)));
            m_resultTable->setItem(i, 4, new QTableWidgetItem(QString::number(new_9x9Points[i].y, 'f', 2)));
        }
        
        cv::Mat homographyMatrix;
        double error = m_core->calibrateHomography(m_9x9Points, preset9_9Points, homographyMatrix);
        if (error >= 0) {
        }
    } else {
        return false;
    }
    return true;
}

bool MirrorCalibFunction::Save3x3Result(const QString savePath){
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
    
    if (preset3_3Points.size() != m_3x3Points.size() || preset3_3Points.empty()) return false;
        if (!savePath.isEmpty()) {
            m_core->saveComparisonToCSV(savePath, m_3x3Points, preset3_3Points);
        }
    return true;
}

QVector<QPointF> MirrorCalibFunction::Get9x9Points() const {
    QVector<QPointF> points;
    for(const auto& pt : m_9x9Points) {
        points.push_back(QPointF(pt.x, pt.y));
    }
    return points;
}

IMirrorScene* MirrorCalibFunction::getScene() const 
{
    return m_MirrorScene;
}

CalibWarptivePort* newCalibWarptivePort(){
    return new MirrorCalibFunction();
}

