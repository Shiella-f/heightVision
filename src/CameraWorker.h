#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QImage>
#include <opencv2/opencv.hpp>
#include "camerapara.h" // For CameraPara struct if needed, or just redefine parameters

class CameraWorker : public QObject {
    Q_OBJECT

public:
    explicit CameraWorker(QObject* parent = nullptr);
    ~CameraWorker();

    void setParams(const CameraPara::Camerapara& params);
    void requestWork();
    void abort();

public slots:
    void doWork();
    void updateParams(const CameraPara::Camerapara& params);

signals:
    void frameReady(const cv::Mat& frame);
    void finished();
    void error(QString err);

private:
    cv::VideoCapture m_cap;
    bool m_abort;
    bool m_working;
    QMutex m_mutex;
    CameraPara::Camerapara m_params;
    bool m_paramsDirty;

    void process();
    void applyParameters();
};
