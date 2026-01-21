#include "UDPMatReceiver.h"
#include <QDebug>

UDPMatReceiver::UDPMatReceiver(QObject* parent)
    : QObject(parent) {
    reconnectTimer.setInterval(1000);
    reconnectTimer.setSingleShot(false);

    connect(&sock, &QUdpSocket::readyRead, this, &UDPMatReceiver::onReadyRead);

    connect(&sock,
            QOverload<QAbstractSocket::SocketError>::of(&QUdpSocket::error),
            this,
            &UDPMatReceiver::onError);
    connect(&reconnectTimer, &QTimer::timeout, this, &UDPMatReceiver::tryConnect);
}

UDPMatReceiver::~UDPMatReceiver() {
    stop();
}

void UDPMatReceiver::registerMetaTypes() {
    qRegisterMetaType<cv::Mat>("cv::Mat");
}

bool UDPMatReceiver::start(const QString& host, quint16 port) {
    stop(); // 先清理旧状态
    targetHost = host;
    targetPort = port;
    waitingFirstFrame = true;
    expectedFrameLen = 0;
    buffer.clear();
    lastFrameTimer.invalidate();
    tryConnect();
    if (!reconnectTimer.isActive()) {
        reconnectTimer.start();
        return true;
    }
    return false;
}

void UDPMatReceiver::stop() {
    reconnectTimer.stop();
    sock.close();
    buffer.clear();
    expectedFrameLen = 0;
    waitingFirstFrame = true;
    lastFrameTimer.invalidate();
}

void UDPMatReceiver::tryConnect() {
    // UDP 端不需要真正“连接”，但需要先绑定本地端口并主动发送握手包让服务器记录地址
    if (sock.state() != QAbstractSocket::BoundState) {
        // 增大接收缓冲区到 20MB (默认可能只有 64KB)，解决高清图传输丢包导致的卡顿/花屏
        sock.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 20 * 1024 * 1024);

        if (!sock.bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            emit statusText(QStringLiteral("udp bind 失败: %1").arg(sock.errorString()));
            return;
        }
    }

    // 仅在还未收到第一帧，或长时间未收帧时重发握手，避免频繁打扰服务器
    bool shouldHandshake = waitingFirstFrame;
    if (!waitingFirstFrame && (!lastFrameTimer.isValid() || lastFrameTimer.elapsed() > 2000)) {
        shouldHandshake = true;
    }

    if (!shouldHandshake) {
        return;
    }

    QByteArray hello(1, '\0'); // 任意内容，服务器只需知道客户端地址/端口
    qint64 sent = sock.writeDatagram(hello, QHostAddress(targetHost), targetPort);
    if (sent < 0) {
        emit statusText(QStringLiteral("握手包发送失败: %1").arg(sock.errorString()));
    } else {
        emit statusText(QStringLiteral("已发送 UDP 连接到 %1:%2").arg(targetHost).arg(targetPort));
    }
}

void UDPMatReceiver::onError(QAbstractSocket::SocketError) {
    emit statusText(QStringLiteral("socket error: %1").arg(sock.errorString()));
}

void UDPMatReceiver::onReadyRead() {
    while (sock.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(sock.pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        qint64 read = sock.readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        if (read <= 0) continue;

        // 1. 尝试识别帧头（长度包）
        // 服务器发送的长度包固定为 4 字节
        bool isHeader = false;
        if (read == 4) {
            quint32 beLen = 0;
            memcpy(&beLen, datagram.constData(), 4);
            quint32 len = qFromBigEndian(beLen);

            // 简单的合理性校验 (例如 1KB ~ 50MB)
            // 这是一个启发式判断：如果收到的 4 字节解析出来像是一个合理的帧长度，
            // 我们就认为它是新的一帧的开始。
            if (len > 1024 && len < 50 * 1024 * 1024) {
                isHeader = true;
                
                // 开始新的一帧
                expectedFrameLen = len;
                buffer.clear();
                buffer.reserve(static_cast<int>(len));
                
                if (waitingFirstFrame) {
                    waitingFirstFrame = false;
                    emit statusText(QStringLiteral("收到首帧，长度: %1").arg(len));
                }
                lastFrameTimer.restart();
            }
        }

        // 2. 如果不是帧头，且我们正在等待数据，则追加
        if (!isHeader && expectedFrameLen > 0) {
            buffer.append(datagram);

            // 3. 检查是否收满
            if (buffer.size() >= static_cast<int>(expectedFrameLen)) {
                // 尝试解码
                // 注意：buffer 可能比 expectedFrameLen 大（如果 UDP 乱序或逻辑重叠），
                // 但 imdecode 只会读取它需要的部分。
                
                cv::Mat frame = cv::imdecode(
                    std::vector<uchar>(buffer.begin(), buffer.begin() + expectedFrameLen), 
                    cv::IMREAD_COLOR
                );

                if (!frame.empty()) {
                    emit frameReady(frame);
                    lastFrameTimer.restart();
                } 
                
                // 重置，等待下一帧头
                expectedFrameLen = 0;
                buffer.clear();
            }
        }
    }
}

bool UDPMatReceiver::processBuffer() {
    return false; 
}
