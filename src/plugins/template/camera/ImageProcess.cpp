#include "ImageProcess.h"
#include <QTimer>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>

ImageProcess::ImageProcess()
{
}

cv::Mat ImageProcess::CalibImage(cv::Mat &originalImage)
{
    if(!originalImage.empty())
    {
        cv::Mat frame = originalImage.clone();
        // 2. 如果已加载标定数据，进行去畸变
        cv::Mat displayedFrame = frame;
        if (m_isCameraCalibLoaded) 
        {
            // 如果图像尺寸改变，需要重新初始化映射表
            if (frame.size() != m_calibImageSize) 
            {
                m_calibImageSize = frame.size();
                // 计算新的相机矩阵，alpha=0 裁剪黑边，alpha=1 保留所有像素
                cv::Mat newCameraMatrix = cv::getOptimalNewCameraMatrix(m_cameraMatrix, m_distCoeffs, m_calibImageSize, 1, m_calibImageSize, 0);
                cv::initUndistortRectifyMap(m_cameraMatrix, m_distCoeffs, cv::Mat(), newCameraMatrix, m_calibImageSize, CV_16SC2, m_map1, m_map2);
            }
            cv::Mat undistortedFrame;
            cv::remap(frame, undistortedFrame, m_map1, m_map2, cv::INTER_LINEAR);
            displayedFrame = undistortedFrame;
            return displayedFrame;
        }
        else
        {
            return displayedFrame;
        }
    } else 
    {
        qInfo().noquote() << "image is empty";
        return cv::Mat();
    }
}

cv::Mat ImageProcess::WarPerspective(cv::Mat& originalImage){
    cv::Mat displayedFrame;
    if (!m_TiltParams.hasMap_) {
        qDebug() << "[CameraCorrector] 请先调用 computeMap()。\n";
        return cv::Mat();
    }
    if (originalImage.empty())
        return cv::Mat();
    cv::warpPerspective(originalImage, displayedFrame,m_TiltParams.homography,
        m_config.outSize(),cv::INTER_LINEAR,cv::BORDER_CONSTANT,cv::Scalar::all(0));
    return displayedFrame;
}

cv::Mat ImageProcess::apply3x3Calibration(const cv::Mat& originalImage)
{
    if(originalImage.empty()) {
        qInfo().noquote() << "输入图像为空，无法应用3x3标定";
        return cv::Mat();
    }
    if (!m_is3_3CalibLoaded) {
        qInfo().noquote() << "3x3标定数据未加载，返回原图";
        return originalImage;
    }

    if (roi_3x3.width() > 0 && roi_3x3.height() > 0 &&
        roi_3x3.x() >= 0 && roi_3x3.y() >= 0 &&
        roi_3x3.x() + roi_3x3.width() <= originalImage.cols &&
        roi_3x3.y() + roi_3x3.height() <= originalImage.rows) {
        return originalImage(cv::Rect(roi_3x3.x(), roi_3x3.y(), roi_3x3.width(), roi_3x3.height())).clone();
    } else {
        qInfo().noquote() << "ROI区域超出图像边界，返回原图";
        return originalImage;
    }
}

bool ImageProcess::loadCameraCalibration(const QString& calibFilePath){
    if(calibFilePath.isEmpty()) {
        qInfo().noquote() << "未选择相机标定文件，跳过该步骤";
        m_isCameraCalibLoaded = false;
        return false;
    }
    QFile file(calibFilePath);
    if (!file.exists()) {
        qInfo().noquote() << "相机标定文件不存在，跳过该步骤";
        m_isCameraCalibLoaded = false;
        return false;
    } else {
        try {
            cv::FileStorage fs(calibFilePath.toStdString(), cv::FileStorage::READ);
            if (!fs.isOpened()) {
                qInfo().noquote() << "无法打开相机标定文件";
                m_isCameraCalibLoaded = false;
                return false;
            } else {
                fs["camera_matrix"] >> m_cameraMatrix;
                fs["distortion_coefficients"] >> m_distCoeffs;
                fs.release();
                if (m_cameraMatrix.empty() || m_distCoeffs.empty()) {
                    qInfo().noquote() << "相机标定文件数据无效";
                    m_isCameraCalibLoaded = false;
                    return false;
                } else {
                    m_isCameraCalibLoaded = true;
                    return true;
                    qInfo().noquote() << "相机标定数据加载成功！";
                }
            }
        } catch (cv::Exception& e) {
            qInfo().noquote() << QString("加载标定数据异常: %1").arg(e.what());
            m_isCameraCalibLoaded = false;
            return false;
        }
    }
}

bool ImageProcess::loadTiltCalibration(const QString& homographyPath)
{
    if (homographyPath.isEmpty()) {
        qInfo().noquote() << "未选择相机校正文件，跳过该步骤";
        return false;
    } else {
        try{
            cv::FileStorage fs(std::string(homographyPath.toStdString()), cv::FileStorage::READ);
            if (!fs.isOpened()) return false;
            fs["gridCols"]    >> m_config.gridCols;
            fs["gridRows"]    >> m_config.gridRows;
            fs["squareMm"]    >> m_config.squareMm;
            fs["outWidthMm"]  >> m_config.outWidthMm;
            fs["outHeightMm"] >> m_config.outHeightMm;
            fs["pixPerMm"]    >> m_config.pixPerMm;
            if (!fs["homography"].empty())
            {
                fs["homography"] >> m_TiltParams.homography;
                m_TiltParams.hasMap_ = !m_TiltParams.homography.empty();
                return m_TiltParams.hasMap_;
            }else
            {
                m_TiltParams.hasMap_ = false;
                return false;
            }
            fs.release();
        }
        
        catch(const std::exception& e)
        {
            qInfo().noquote() << QString("加载相机校正数据异常: %1").arg(e.what());
            m_TiltParams.hasMap_ = false;
            return false;
        }
    }
}

bool ImageProcess::load3x3Calibration(const QString& calibPath2)
{
    if(calibPath2.isEmpty()) {
        qInfo().noquote() << "未选择3*3振镜标定文件，跳过该步骤";
        m_is3_3CalibLoaded = false;
        return false;
    }
    if (!calibPath2.isEmpty()) {
        QFile roiFile(calibPath2);
        if (!roiFile.exists() || !roiFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qInfo().noquote() << "3*3振镜标定文件不存在或无法打开";
            m_is3_3CalibLoaded = false;
            return false;
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
                qInfo().noquote() << QString("3*3振镜ROI加载成功: X=%1, Y=%2, W=%3, H=%4").arg(x).arg(y).arg(w).arg(h);
                m_is3_3CalibLoaded = true;
            } else {
                qInfo().noquote() << "3*3振镜标定文件格式错误，未找到完整的ROI信息";
                m_is3_3CalibLoaded = false;
            }
        }
    }
    return m_is3_3CalibLoaded;
}

bool ImageProcess::load9x9Calibration(const QString& calibFilePath)
{
    if (calibFilePath.isEmpty()) {
        qInfo().noquote() << "未选择9*9振镜标定文件，跳过该步骤";
        m_is9_9CalibLoaded = false;
    } else {
        QFile Matrixfile(calibFilePath);
        if (!Matrixfile.exists()) {
            qInfo().noquote() << "9*9振镜标定文件不存在，跳过该步骤";
            m_is9_9CalibLoaded = false;
        } else {
            try {
                // 读取 9x9 振镜单应矩阵
                cv::FileStorage fs(calibFilePath.toStdString(), cv::FileStorage::READ);
                if (!fs.isOpened()) {
                    qInfo().noquote() << "无法打开9*9振镜标定文件，跳过该步骤";
                    m_is9_9CalibLoaded = false;
                } else {
                    fs["HomographyMatrix"] >> HomographyMatrix;
                    fs.release();

                    if (HomographyMatrix.empty()) {
                        qInfo().noquote() << "9*9振镜标定文件数据无效";
                        m_is9_9CalibLoaded = false;
                    } else {
                        m_is9_9CalibLoaded = true;
                        qInfo().noquote() << "9*9振镜标定数据加载成功！";
                    }
                }
            } catch (cv::Exception& e) {
                qInfo().noquote() << QString("加载标定数据异常: %1").arg(e.what());
                m_is9_9CalibLoaded = false;
            }
        }
    }
    return m_is9_9CalibLoaded;
}

void ImageProcess::loadCalibrationData()
{
    int ret1 = QMessageBox::question(nullptr,
                                     "相机标定",
                                     "是否选择相机标定文件？",
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    QString calibPath = nullptr;
    if (ret1 == QMessageBox::Yes) {
        calibPath = QFileDialog::getOpenFileName(nullptr, ("选择相机标定文件"), "", ("Images (*.xml);;All Files (*)"));
        loadCameraCalibration(calibPath);
    }
    
    int ret4 = QMessageBox::question(nullptr,
                                     "相机校正",
                                     "是否选择相机校正文件？",
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    QString homographyPath = nullptr;
    if (ret4 == QMessageBox::Yes) {
        homographyPath = QFileDialog::getOpenFileName(nullptr, ("选择相机校正文件"), "", ("Images (*.yaml);;All Files (*)"));
        loadTiltCalibration(homographyPath);
    }
    
    int ret2 = QMessageBox::question(nullptr,
                                     "振镜标定",
                                     "是否选择3*3振镜标定文件(ROI)？",
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    QString calibPath2;
    if (ret2 == QMessageBox::Yes) {
        calibPath2 = QFileDialog::getOpenFileName(nullptr, ("选择3*3振镜标定文件(ROI)"), "", ("Text Files (*.txt);;All Files (*)"));
        load3x3Calibration(calibPath2);
    }
    
    int ret3 = QMessageBox::question(nullptr,
                                     "9*9振镜标定",
                                     "是否选择9*9振镜标定文件？",
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    QString calibMatrixPath = nullptr;
    if (ret3 == QMessageBox::Yes) {
        calibMatrixPath = QFileDialog::getOpenFileName(nullptr, ("选择9*9振镜标定文件"), "", ("Images (*.xml);;All Files (*)"));
        load9x9Calibration(calibMatrixPath);
    } 
}

bool ImageProcess::m_isTiltLoaded() const
{
    return m_TiltParams.hasMap_;
}

double ImageProcess::mmPixel() const
{
    if (m_TiltParams.hasMap_) {
        // 假设 mmPerPixel 是通过某种方式计算得到的
         return 1.0 / m_config.pixPerMm;
    }
    return -1.0; // 返回 -1 表示失败
}

void ImageProcess::resetPhysicalOffset()
{
    physicalOffsetMm_ = {0.0, 0.0};
}

bool ImageProcess::setCoordinateOriginByPair(cv::Point2d pixelPt,
                                                cv::Point2d physicalPtMm)
{
    if (m_config.pixPerMm <= 0.0)
    {
        std::cerr << "[BoardHomography] pixPerMm 非法，无法设置二次初始化偏移。\n";
        return false;
    }

    const cv::Point2d baseMm(pixelPt.x / m_config.pixPerMm,
                             pixelPt.y / m_config.pixPerMm);
    physicalOffsetMm_ = physicalPtMm - baseMm;

    std::cout << "[BoardHomography] 二次初始化偏移(mm): ("
              << physicalOffsetMm_.x << ", "
              << physicalOffsetMm_.y << ")\n";
    return true;
}

ImageProcessPort* newImageProcessPort() {
    return new ImageProcess();
}

