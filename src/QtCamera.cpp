#include "QtCamera.h"
#include <QCameraInfo>
#include <QDebug>
#include <QVideoProbe>
#include <QVideoFrame>
#include <QImage>

QtCamera::QtCamera(QWidget *parent)
{
    // 初始化成员变量
    m_camera = nullptr;
    // viewfinder 在 initUI 中创建
    m_viewfinder = nullptr;
    m_videoProbe = nullptr;
    m_layout = nullptr;

    initUI();
}

QtCamera::~QtCamera()
{
    closeCamera();
}

void QtCamera::initUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    // 设置 Viewfinder 用于预览
    m_viewfinder = new QCameraViewfinder(this);
    m_layout->addWidget(m_viewfinder);

    // 创建 VideoProbe 对象，用于拦截和处理视频帧
    m_videoProbe = new QVideoProbe(this);
    connect(m_videoProbe, &QVideoProbe::videoFrameProbed, this, &QtCamera::processVideoFrame);
}

bool QtCamera::openCamera(int deviceIndex)
{
    closeCamera(); // 先关闭当前可能打开的摄像头

    // 获取可用摄像头列表
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    if (cameras.isEmpty()) {
        emit errorOccurred("No cameras found.");
        return false;
    }

    if (deviceIndex < 0 || deviceIndex >= cameras.size()) {
        emit errorOccurred("Invalid camera index.");
        return false;
    }

    // 创建摄像头对象
    m_camera = new QCamera(cameras.at(deviceIndex), this);
    
    // 设置 Viewfinder
    m_camera->setViewfinder(m_viewfinder);
    
    // 将 probe 连接到摄像头
    if (!m_videoProbe->setSource(m_camera)) {
        qWarning() << "Could not set video probe source. Frame capture may not work.";
        // 不一定是致命错误，预览可能仍然工作
    }

    // 启动摄像头
    m_camera->start();

    if (m_camera->status() == QCamera::Status::UnavailableStatus) {
         emit errorOccurred("Camera unavailable.");
         return false;
    }
    
    return true;
}

void QtCamera::closeCamera()
{
    if (m_camera) {
        m_camera->stop();
        m_videoProbe->setSource((QMediaObject*)nullptr); // 断开 Probe
        delete m_camera;
        m_camera = nullptr;
    }
}

bool QtCamera::isCapturing() const
{
    return m_camera && m_camera->status() == QCamera::ActiveStatus;
}

void QtCamera::processVideoFrame(const QVideoFrame &frame)
{
    QVideoFrame cloneFrame(frame);
    if (cloneFrame.map(QAbstractVideoBuffer::ReadOnly)) {
        
        QImage::Format format = QVideoFrame::imageFormatFromPixelFormat(cloneFrame.pixelFormat());
        
        if (format != QImage::Format_Invalid) {
            QImage image(cloneFrame.bits(), 
                         cloneFrame.width(), 
                         cloneFrame.height(), 
                         cloneFrame.bytesPerLine(), 
                         format);
                         
            // QVideoFrame map 的内存只在 map 期间有效，需要深拷贝
            // 另外很多摄像头可能是镜像的，这里可以做镜像翻转处理
            // image = image.mirrored(true, false); 

            emit imageCaptured(image.copy()); 
        } else {
             // 如果格式不支持直接转换，可能需要额外的转换步骤，例如使用 QVideoFrame::pixelFormat() 判断具体格式（如NV12, YUYV等）并手动转换
             // 这里简单处理，如果无效则忽略
             qWarning() << "Unsupported frame format for direct conversion:" << cloneFrame.pixelFormat();
        }
        
        cloneFrame.unmap();
    }
}
