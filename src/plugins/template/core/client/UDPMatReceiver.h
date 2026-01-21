#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QByteArray>
#include <QtEndian>
#include <QString>
#include <QElapsedTimer>
#include <QHostAddress>
#include <opencv2/opencv.hpp>
#include <vector>

Q_DECLARE_METATYPE(cv::Mat)

class UDPMatReceiver : public QObject {
    Q_OBJECT

public:
    explicit UDPMatReceiver(QObject* parent = nullptr);
    ~UDPMatReceiver() override;

    static void registerMetaTypes();

public slots:
    bool start(const QString& host = QStringLiteral("0.0.0.0"), quint16 port = 9000);

    void stop();

signals:
    void frameReady(cv::Mat frame);
    void statusText(QString text);

private slots:
    void tryConnect();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError err);

private:
    bool processBuffer(); // 返回是否解出至少一帧

private:
    QUdpSocket sock; // UDP socket（客户端主动发一个握手包即可收到推流）
    QTimer reconnectTimer; // 定时尝试重发握手/保活
    QElapsedTimer lastFrameTimer; // 记录上次收到帧的时间，用于决定何时重发握手

    QByteArray buffer; // 接收缓冲区（UDP 分片重新组包）
    quint32 expectedFrameLen = 0; // 当前待收完整帧的长度

    QString targetHost = QStringLiteral("0.0.0.0");
    quint16 targetPort = 9000;
    bool waitingFirstFrame = true; // 还未收到第一帧时保持握手
};
