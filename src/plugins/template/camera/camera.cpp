#include "camera.h"
#include <QTimer>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>

MyCamera::MyCamera()
{

}
MyCamera::~MyCamera()
{

}

void MyCamera::usbCamera()
{
    cap.open(1); // 打开默认摄像头
    if (!cap.isOpened()) {
        qWarning().noquote() << "无法打开摄像头";
        return;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
    qInfo().noquote() << "USB Camera opened successfully.";
    cap.set(cv::CAP_PROP_AUTOFOCUS, 0);
    cap.set(cv::CAP_PROP_FOCUS, 350);
    // m_cameraTimer = new QTimer(this);
    // connect(m_cameraTimer, &QTimer::timeout, this, &MyCamera::updateCameraFrame);
    // m_cameraTimer->start(66);
}

cv::Mat MyCamera::updateCameraFrame()
{
    if(cap.isOpened())
    {
        cv::Mat frame;
        cap >> frame; // 从摄像头捕获一帧
        if (frame.empty()) 
        {
            cv::Mat blackFrame(1080, 1920, CV_8UC3, cv::Scalar(0, 0, 0));
            return blackFrame;
        } else 
        {
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
                if(roi_3x3.width() > 0 && roi_3x3.height() > 0 &&
                   roi_3x3.x() >= 0 && roi_3x3.y() >= 0 &&
                   roi_3x3.x() + roi_3x3.width() <= undistortedFrame.cols &&
                   roi_3x3.y() + roi_3x3.height() <= undistortedFrame.rows) 
                {
                    displayedFrame = undistortedFrame( cv::Rect(roi_3x3.x(), roi_3x3.y(), roi_3x3.width(), roi_3x3.height()) ).clone();
                } else 
                {
                    displayedFrame = undistortedFrame;
                }

            }

            // 3. 转换颜色并显示在主界面 (显示去畸变后的图像)
            cv::cvtColor(displayedFrame, displayedFrame, cv::COLOR_BGR2RGB);
            return displayedFrame;
        }
    } else {
        qInfo().noquote() << "Camera handle or VideoCapture not initialized.";
        return cv::Mat();
    }
}

void MyCamera::loadCalibrationData()
{

    int ret1 = QMessageBox::question(nullptr,
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
        qInfo().noquote() << "相机标定文件不存在或未选择标定文件";
        return;
    }

    try {
        cv::FileStorage fs(calibPath.toStdString(), cv::FileStorage::READ);
        if (!fs.isOpened()) {
            qInfo().noquote() << "无法打开相机标定文件";
            return;
        }

        fs["camera_matrix"] >> m_cameraMatrix;
        fs["distortion_coefficients"] >> m_distCoeffs;
        fs.release();

        if (m_cameraMatrix.empty() || m_distCoeffs.empty()) {
            qInfo().noquote() << "相机标定文件数据无效";
            m_isCameraCalibLoaded = false;
        } else {
            m_isCameraCalibLoaded = true;
            qInfo().noquote() << "相机标定数据加载成功！";
        }
    } catch (cv::Exception& e) {
        qInfo().noquote() << QString("加载标定数据异常: %1").arg(e.what());
        m_isCameraCalibLoaded = false;
    }


    int ret2 = QMessageBox::question(nullptr,
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
            qInfo().noquote() << "3*3振镜标定文件不存在或无法打开";
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

    int ret3 = QMessageBox::question(nullptr,
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
        qInfo().noquote() << "9*9振镜标定文件不存在或未选择标定文件";
        return;
    }

    try {
        cv::FileStorage fs(calibPath.toStdString(), cv::FileStorage::READ);
        if (!fs.isOpened()) {
            qInfo().noquote() << "无法打开9*9振镜标定文件";
            return;
        }

        fs["HomographyMatrix"] >> HomographyMatrix;
        fs.release();

        if (HomographyMatrix.empty()) {
            qInfo().noquote() << "9*9振镜标定文件数据无效";
            m_is9_9CalibLoadedr = false;
        } else {
            m_is9_9CalibLoadedr = true;
            qInfo().noquote() << "9*9振镜标定数据加载成功！";
        }
    } catch (cv::Exception& e) {
        qInfo().noquote() << QString("加载标定数据异常: %1").arg(e.what());
        m_is9_9CalibLoadedr = false;
    }

}
