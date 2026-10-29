#include "ImageProcess.h"
#include <QTimer>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include "../../Calib/MeasurementCalibration.h"
#include "../../Calib/PlaneModel.h"

ImageProcess::ImageProcess()
{
}

cv::Mat ImageProcess::CalibImage(cv::Mat &originalImage)
{
    if(!originalImage.empty())
    {
        cv::Mat frame = originalImage;
        // 2. 如果已加载标定数据，进行去畸变
        cv::Mat displayedFrame = frame;
        if (m_isCameraCalibLoaded) 
        {
            // 如果图像尺寸改变，需要重新初始化映射表
            if (frame.size() != m_calibImageSize) {
                qWarning()<<"相机原图尺寸与标定参数不一致，未执行去畸变。";return {};
            }
            if (m_map1.empty() || m_map2.empty())
            {
                // 计算新的相机矩阵，alpha=0 裁剪黑边，alpha=1 保留所有像素
                cv::initUndistortRectifyMap(m_cameraMatrix, m_distCoeffs, cv::Mat(), m_newCameraMatrix, m_calibImageSize, CV_16SC2, m_map1, m_map2);
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
    if(m_tiltCalibration.plane){
        const auto& model=*m_tiltCalibration.plane;cv::Mat base;
        if(originalImage.empty()||originalImage.size()!=model.imageSize)return {};
        if((m_planeMapX.empty()||m_planeMapY.empty())&&!model.buildMaps(m_planeMapX,m_planeMapY))return {};
        cv::remap(originalImage,base,m_planeMapX,m_planeMapY,cv::INTER_LINEAR,cv::BORDER_CONSTANT);
        return model.postRoi.apply(base,displayedFrame)?displayedFrame:cv::Mat();
    }
    if (!m_TiltParams.hasMap_) {
        qDebug() << "[CameraCorrector] 请先调用 computeMap()。\n";
        return cv::Mat();
    }
    if (originalImage.empty())
        return cv::Mat();
    cv::warpPerspective(originalImage, displayedFrame,m_TiltParams.homography,
        m_config.outSize(),cv::INTER_LINEAR,cv::BORDER_CONSTANT,cv::Scalar::all(0));
    cv::Mat result;return m_tiltCalibration.postRoi.apply(displayedFrame,result)?result:cv::Mat();
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

bool ImageProcess::loadCameraCalibration(const QString& path) {
    m_TiltParams={};m_tiltCalibration={};m_planeMapX.release();m_planeMapY.release();
    m_isCameraCalibLoaded=false;m_map1.release();m_map2.release();
    m_cameraMatrix.release();m_distCoeffs.release();m_newCameraMatrix.release();m_calibImageSize={};
    MeasurementCalibration calibration;
    if(!calibration.loadCamera(path)){
        qWarning()<<"?????????????????K?D?K_new????????";return false;
    }
    m_cameraMatrix=calibration.cameraMatrix;m_distCoeffs=calibration.distCoeffs;
    m_newCameraMatrix=calibration.newCameraMatrix;m_calibImageSize=calibration.imageSize;
    m_isCameraCalibLoaded=true;return true;
}

bool ImageProcess::loadTiltCalibration(const QString& homographyPath)
{
    m_TiltParams={};m_tiltCalibration={};m_planeMapX.release();m_planeMapY.release();resetPhysicalOffset();
    MeasurementCalibration candidate;candidate.imageSize=m_calibImageSize;candidate.cameraMatrix=m_cameraMatrix;
    candidate.distCoeffs=m_distCoeffs;candidate.newCameraMatrix=m_newCameraMatrix;
    if(homographyPath.isEmpty()||!candidate.loadPerspective(homographyPath))return false;
    if(candidate.plane){m_tiltCalibration=std::move(candidate);m_TiltParams.hasMap_=true;m_isCameraCalibLoaded=false;return true;}
    if(candidate.outputPixels().empty())return false;
    m_config.outWidthMm=candidate.outputMm.width;m_config.outHeightMm=candidate.outputMm.height;m_config.pixPerMm=candidate.pixPerMm;
    physicalOffsetMm_=candidate.offsetMm;m_TiltParams.homography=candidate.renderHomography();m_TiltParams.hasMap_=true;
    if(candidate.hasCamera()){
        m_calibImageSize=candidate.imageSize;m_cameraMatrix=candidate.cameraMatrix;m_distCoeffs=candidate.distCoeffs;m_newCameraMatrix=candidate.newCameraMatrix;
        m_map1.release();m_map2.release();m_isCameraCalibLoaded=true;
    }
    if(candidate.postRoi.enabled)physicalOffsetMm_={0,0};
    m_tiltCalibration=std::move(candidate);return true;
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
    const auto& roi=m_tiltCalibration.plane?m_tiltCalibration.plane->postRoi:m_tiltCalibration.postRoi;
    if(roi.enabled)return 300./roi.outputSize.width;
    if(m_tiltCalibration.plane)return m_tiltCalibration.plane->regionSize.width/m_tiltCalibration.plane->outputSize.width;
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
    if(m_tiltCalibration.postRoi.enabled||(m_tiltCalibration.plane&&m_tiltCalibration.plane->postRoi.enabled))return false;
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

cv::Mat ImageProcess::enhanceResultImage(const cv::Mat& src)
{
    if (src.empty()) return src;

    cv::Mat contrast = src.clone();

    // 局部对比度增强，适合纹理/边缘不明显的图
    if (src.channels() == 3) {
        cv::Mat lab;
        cv::cvtColor(src, lab, cv::COLOR_BGR2Lab);

        std::vector<cv::Mat> ch;
        cv::split(lab, ch);

        auto clahe = cv::createCLAHE(1.5, cv::Size(8, 8));
        clahe->apply(ch[0], ch[0]);

        cv::merge(ch, lab);
        cv::cvtColor(lab, contrast, cv::COLOR_Lab2BGR);
    }

    // 轻度反遮罩锐化
    cv::Mat blurred, sharp;
    cv::GaussianBlur(contrast, blurred, cv::Size(0, 0), 1.0);
    cv::addWeighted(contrast, 1.45, blurred, -0.45, 0, sharp);

    return sharp;
}
