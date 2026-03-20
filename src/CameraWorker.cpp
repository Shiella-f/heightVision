#include "CameraWorker.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>

CameraWorker::CameraWorker(QObject *parent)
    : QObject(parent), m_abort(false), m_working(false), m_paramsDirty(false)
{
    qRegisterMetaType<cv::Mat>("cv::Mat");
    // qRegisterMetaType can be used for custom types in signals/slots
}

CameraWorker::~CameraWorker()
{
}

void CameraWorker::requestWork()
{
    m_working = true;
    m_abort = false;
    emit doWork(); 
}

void CameraWorker::abort()
{
    m_abort = true;
    m_working = false;
}

void CameraWorker::setParams(const CameraPara::Camerapara& params) {
    QMutexLocker locker(&m_mutex);
    m_params = params;
    m_paramsDirty = true;
}

void CameraWorker::updateParams(const CameraPara::Camerapara& params)
{
    QMutexLocker locker(&m_mutex);
    m_params = params;
    m_paramsDirty = true;
}

void CameraWorker::doWork()
{
    qDebug() << "Worker started in thread " << QThread::currentThreadId();
    
    // Open camera here in the thread
    if (!m_cap.isOpened()) {
        m_cap.open(1, cv::CAP_DSHOW); // Default camera, try DSHOW for Windows if standard fails or for better control
        if (!m_cap.isOpened()) {
            m_cap.open(0); // Fallback
        }
    }
    
    if (!m_cap.isOpened()) {
        emit error("Failed to open camera");
        m_working = false;
        return;
    }
    
    // Basic setup
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
    applyParameters();

    while (m_working) {
        if (m_abort) break;

        // Apply parameter updates
        bool dirty = false;
        {
            QMutexLocker locker(&m_mutex);
            if(m_paramsDirty) {
               dirty = true;
               m_paramsDirty = false;
            }
        }
        
        if(dirty) {
             applyParameters();
        }

        cv::Mat frame;
        if(m_cap.read(frame)) {
           if(!frame.empty()) {
               emit frameReady(frame.clone());
           }
        } else {
             QThread::msleep(10);
        }
        
        QCoreApplication::processEvents();
    }
    
    if(m_cap.isOpened()) {
        m_cap.release();
    }
    emit finished();
}

void CameraWorker::process()
{
    // Implementation placeholder
}

void CameraWorker::applyParameters() {
    if (!m_cap.isOpened()) return;
    
    QMutexLocker locker(&m_mutex); // Protect m_params read if needed, though usually copied

    // Implement parameter setting logic matching CameraPara::OpenCamera but using m_cap.set
    // Note: Some properties might need reopening the camera or special handling on some backends
    
    // Auto Exposure
    m_cap.set(cv::CAP_PROP_AUTO_EXPOSURE, m_params.autoExposureSupported ? 3 : 1); // 3=Auto, 1=Manual for DSHOW usually
    // Fallback to old logic if that fails? 
    // The original code used 1 : 0. 
    // m_cap.set(cv::CAP_PROP_AUTO_EXPOSURE, m_params.autoExposureSupported ? 1 : 0);
    
    if(!m_params.autoExposureSupported) {
        m_cap.set(cv::CAP_PROP_EXPOSURE, m_params.exposureValue);
    }
    
    m_cap.set(cv::CAP_PROP_GAIN, m_params.gainValue);
    m_cap.set(cv::CAP_PROP_BRIGHTNESS, m_params.brightnessValue);
    m_cap.set(cv::CAP_PROP_CONTRAST, m_params.contrastValue);
    m_cap.set(cv::CAP_PROP_SATURATION, m_params.saturationValue);
    m_cap.set(cv::CAP_PROP_HUE, m_params.toneValue);
    m_cap.set(cv::CAP_PROP_GAMMA, m_params.gammaValue);
    
    // FPS often requires reopen, skipping dynamically for now unless critical
    // m_cap.set(cv::CAP_PROP_FPS, m_params.fpsValue); 
    
    m_cap.set(cv::CAP_PROP_AUTOFOCUS, m_params.autoFocusSupported ? 1 : 0);
    if(!m_params.autoFocusSupported) {
        m_cap.set(cv::CAP_PROP_FOCUS, m_params.focusValue);
    }
}
