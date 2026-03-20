#pragma once

#include <QWidget>
#include <QCamera>
#include <QCameraInfo>
#include <QCameraViewfinder>
#include <QDebug>
#include <QVideoProbe>
#include <QVBoxLayout>
#include <QList>

class QtCamera : public QWidget
{
    Q_OBJECT
public:
    explicit QtCamera(QWidget *parent = nullptr);
    ~QtCamera();

    // 打开指定索引的摄像头
    bool openCamera(int deviceIndex = 0);
    // 关闭摄像头
    void closeCamera();
    // 检查摄像头是否处于活动状态
    bool isCapturing() const;

signals:
    // 当摄像头捕获到一帧图像时发出此信号 (深拷贝后的 QImage)
    void imageCaptured(const QImage &image);
    // 错误信号
    void errorOccurred(QString errorString);

private slots:
    // 处理探测到的视频帧
    void processVideoFrame(const QVideoFrame &frame);

private:
    void initUI();

    QCamera *m_camera;
    QCameraViewfinder *m_viewfinder;
    QVideoProbe *m_videoProbe;
    QVBoxLayout *m_layout;
};
