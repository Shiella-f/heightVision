#include "matchFunction.h"
#include <QWidget> 
#include <QToolButton> 
#include <QSizePolicy> 
#include <QLabel> 
#include <QSpinBox>
#include <QDoubleSpinBox> 
#include <QCheckBox> 
#include <QVBoxLayout> 
#include <QHBoxLayout> 
#include <QGridLayout> 
#include <QComboBox> 
#include <QGraphicsView> 
#include <QMessageBox> 
#include <QDebug> 
#include <QTimer>
#include <QGroupBox>
#include <QFileDialog>
#include <QSlider>
#include <cmath>
#include <QScrollArea>
#include <QSpacerItem>
#include <QTransform>
#include <QCoreApplication>
#include <QDir>
#include <QStringList> // 使用 QStringList 维护候选路径列表

#include "Scene/ImageDisplayScene.h"
#include "../Scene/MatchScene.h"

matchFunction::matchFunction(QWidget* parent)
    :m_templateManager(),m_hasLearnedTemplate(false),m_lastMatchResults(),
    m_ImageProcess(),m_initParam()
{
    m_ImageDisplayScene = new MatchScene();
    connect(m_ImageDisplayScene, &MatchScene::totalPathPathChanged,this, &matchFunction::learnMatch);
}


matchFunction::~matchFunction()
{
    if (m_ImageProcess) {
        delete m_ImageProcess;
        m_ImageProcess = nullptr;
    }
}

bool matchFunction::hasLearnedTemplate() const
{
    return m_hasLearnedTemplate;
}

// 全图匹配槽实现
std::vector<MatchResult> matchFunction::confirmMatch()
{
    executeMatch(false);
    return m_lastMatchResults;
}
// 学习模板槽实现
bool matchFunction::learnMatch()
{
    if (!m_ImageDisplayScene) {
        return false;
    }

    cv::Mat templateMat = m_ImageDisplayScene->extractTemplate();
    if (templateMat.empty()) {
        return false;
    }

    if (!m_templateManager.learnTemplate(m_currentImage, templateMat, m_MatchParams)) {
        return false;
    }
    m_learnedFeaturePoints.clear();
    m_learnedKeypoints.clear();
    m_learnedFeaturePoints = m_templateManager.currentFeaturePoints();
    m_learnedKeypoints.reserve(m_learnedFeaturePoints.size());
    for(const auto& pt : m_learnedFeaturePoints){
        m_learnedKeypoints.emplace_back(pt, 1.0f);
    }

    m_hasLearnedTemplate = true;

    if (m_ImageDisplayScene) {
        QRect roi = static_cast<MatchScene*>(m_ImageDisplayScene)->getTemplateBoundingRect(); // 获取 ROI
        if (!roi.isEmpty()) {
            m_ImageDisplayScene->clearOverlays();

            QPen ptPen(Qt::green);
            ptPen.setWidth(1);
            for (const auto &kp : m_learnedKeypoints) {
                m_ImageDisplayScene->addOverlayPoint(QPointF(roi.x() + kp.pt.x, roi.y() + kp.pt.y), ptPen, 2);
            }
        }
    }
    qInfo().noquote() << "Learned" << m_learnedFeaturePoints.size() << "feature points.";
    return true;
}

void matchFunction::clearToggled()
{
    if (m_ImageDisplayScene) {
        auto ms = static_cast<MatchScene*>(m_ImageDisplayScene); // 转换为 MatchScene
        if (ms) {
            ms->clear(); // 清除场景中的 ROI
        }
        m_ImageDisplayScene->clearOverlays(); // 清除覆盖图形
    }
}

bool matchFunction::buildMatchParams(MatchParams &params)
{
    m_MatchParams = params;
    return true;
}

void matchFunction::executeMatch(bool useRoi = false)
{
    if (m_currentImage.empty()) {
        m_lastMatchResults.clear();
        return;
    }
    
    if (!m_templateManager.hasTemplate()) {
        m_lastMatchResults.clear();
        return;
    }

    if (!useRoi) {
        m_FindMatchParams.Mask.release(); // 全图匹配直接清空掩膜
    }

    m_lastMatchResults = m_templateManager.matchTemplate(m_currentImage, m_FindMatchParams);
    renderMatchResults(m_lastMatchResults);
}

void matchFunction::renderMatchResults(const std::vector<MatchResult> &results)
{
    if(results.empty()) {
        return;
    }
    if (m_ImageDisplayScene) {
        m_ImageDisplayScene->clearOverlays(); // 清除之前的覆盖物
        const std::array<QColor, 3> palette{QColor(0, 220, 0), QColor(255, 215, 0), QColor(255, 128, 0)};
        QPen rectPen(Qt::red);
        rectPen.setWidth(2);
        QPen ptPen(Qt::green);
        ptPen.setWidth(1);
        QPen CertenPen(Qt::red);
        CertenPen.setWidth(2);
        QPen resultsPenRect(Qt::green);
        resultsPenRect.setWidth(2);

        for (const auto &res : results) {
            // 如果有变换后的特征点，计算其最小外接旋转矩形并绘制为多边形
            if (!res.transformedFeaturePoints.empty()) {
                std::vector<cv::Point2f> pts = res.transformedFeaturePoints;
                cv::RotatedRect rr = res.rotatedRect;
                cv::Point2f boxPts[4];
                rr.points(boxPts);
                QPolygonF poly;
                poly.reserve(4);
                for (int i = 0; i < 4; ++i) {
                    QPointF imgPt(boxPts[i].x, boxPts[i].y);
                    poly << imgPt;
                }
                QPolygonF minpoly;
                    minpoly << QPointF(res.Roiarea.x, res.Roiarea.y)
                            << QPointF(res.Roiarea.x + res.Roiarea.width, res.Roiarea.y)
                            << QPointF(res.Roiarea.x + res.Roiarea.width, res.Roiarea.y +  res.Roiarea.height)
                            << QPointF(res.Roiarea.x, res.Roiarea.y + res.Roiarea.height);
                m_ImageDisplayScene->addOverlayPolygon(poly, rectPen);
                m_ImageDisplayScene->addOverlayPolygon(minpoly, resultsPenRect);
                
                for (const auto &p : res.transformedFeaturePoints) {
                    m_ImageDisplayScene->addOverlayPoint(QPointF(p.x, p.y), ptPen, 2.0);
                }
                m_ImageDisplayScene->addOverlayPoint(QPointF(res.center.x, res.center.y), CertenPen, 4.0);
            } else {
                QRect qr(res.Roiarea.x, res.Roiarea.y, res.Roiarea.width, res.Roiarea.height);
                m_ImageDisplayScene->addOverlayRect(qr, rectPen);
                m_ImageDisplayScene->addOverlayPoint(QPointF(res.center.x, res.center.y), CertenPen, 4.0);
            }
        }
    }

    const MatchResult &best = results.front();
    for(const auto& res : results){
        qDebug().noquote() << QStringLiteral("匹配结果: 位置=(%1, %2), 角度=%3°,缩放=%4, 分数=%5")
                                .arg(res.center.x, 0, 'f', 2)
                                .arg(res.center.y, 0, 'f', 2)
                                .arg(res.angle, 0, 'f', 2)
                                .arg(res.scale, 0, 'f', 2)
                                .arg(res.score * 100.0, 0, 'f', 1);
    }
     const QString summary = QStringLiteral("共找到 %1 个结果，最高分 %2, 角度 %3°, 最低分 %4")
                                .arg(results.size())
                                .arg(best.score * 100.0)
                                .arg(best.angle)
                                .arg(results.back().score * 100.0);
    qInfo() << summary;
}

bool matchFunction::setcurrentImage(const cv::Mat &image)
{
    if (image.empty()) {
        return false;
    }
    m_currentImage = image.clone();
    QImage qimg; // 临时 QImage 用于显示
     if (!m_currentImage.empty() && TIGER_BSVISION::cvImage2qImage(qimg, m_currentImage)) { 
            m_ImageDisplayScene->setOriginalPixmap(QPixmap::fromImage(qimg));
            m_ImageDisplayScene->setSourceImageSize(qimg.size()); 
            // 确保图片显示在视图中心
    }
    return true;
}

bool matchFunction::setMoveRotateData()
{
    double x = m_cale_initParam.unitedRect.x() + m_cale_initParam.unitedRect.width() / 2.0;
    double y = m_cale_initParam.unitedRect.y() + m_cale_initParam.unitedRect.height() / 2.0;
    initPoint = QPointF(x, y);
    if(m_lastMatchResults.empty()) {
        return false;
    }
    m_MoveRotateData.clear();
    QVector<double> distances;
    distances.reserve(m_lastMatchResults.size());
    for (int i = 0; i < static_cast<int>(m_lastMatchResults.size()); ++i) {
        double dx = m_lastMatchResults[i].center.x - initPoint.x();
        double dy = m_lastMatchResults[i].center.y - initPoint.y();
        double distance = std::sqrt(dx * dx + dy * dy);
        distances.append(distance);
    }
    int disMinIndex = 0;
    for(int i = 1; i < distances.size(); ++i){
        if(distances[i] < distances[disMinIndex]){
            disMinIndex = i;
        }
    }
    for(int i = 0; i < static_cast<int>(m_lastMatchResults.size()); ++i){
        if(i == disMinIndex){
            continue;
        }
        double dx = m_lastMatchResults[i].center.x -m_lastMatchResults[disMinIndex].center.x;
        double dy = m_lastMatchResults[i].center.y - m_lastMatchResults[disMinIndex].center.y;
        double angle = m_lastMatchResults[i].angle - m_lastMatchResults[disMinIndex].angle;
        m_MoveRotateData.addData(QPointF(dx, dy), -angle);
    }


    if(m_MoveRotateData.moveOffsets.size() != m_MoveRotateData.angles.size()){
        return false;
    }
    m_MoveRotateData.totalCounts = m_MoveRotateData.moveOffsets.size();
    return true;
}

void matchFunction::connectUDPReceiver()
{
    auto* m_UDPMatReceiver = new UDPMatReceiver(this);

    connect(m_UDPMatReceiver, &UDPMatReceiver::statusText, this, [](const QString& s){
    qDebug() << s;
    });

    // 注意：cv::Mat 是引用计数对象，跨线程传递一般可用；
    // 但如果你会在接收线程继续复用/修改 frame，建议在槽里 frame.clone()。
    auto udpDataReceived = std::make_shared<bool>(false); // 使用共享指针记录是否收到数据，避免局部变量悬空
    auto udpAttemptCount = std::make_shared<int>(0); // 使用共享指针记录已尝试次数

    connect(m_UDPMatReceiver, &UDPMatReceiver::frameReady, this, [&, udpDataReceived](const cv::Mat& frame){
        *udpDataReceived = true; // 收到首帧标记成功
        cv::Mat local = frame.clone(); // 拷贝一份图像避免线程安全问题
        setcurrentImage(m_ImageProcess->CalibImage(local)); // 校正后更新到界面
    });
    // 注意：0.0.0.0 是服务器监听地址（表示监听所有网卡），客户端发送必须指定具体 IP。
    // 如果服务器在本地，用 "127.0.0.1"；如果在树莓派/其他机器，请输入它的局域网 IP（如 "192.168.1.x"）。
    QTimer* udpRetryTimer = new QTimer(this); // 创建重试定时器
    udpRetryTimer->setInterval(500); // 设置 500ms 间隔
    connect(udpRetryTimer, &QTimer::timeout, this, [this, m_UDPMatReceiver, udpRetryTimer, udpDataReceived, udpAttemptCount]() {
        if (*udpDataReceived) { // 如果已收到数据则认为成功
            qDebug() << "UDP 已收到首帧，停止重试"; // 打印成功日志
            udpRetryTimer->stop(); // 停止定时器
            udpRetryTimer->deleteLater(); // 释放定时器
            return; // 结束本次回调
        }
        if (*udpAttemptCount >= 10) { // 超过 5 次未收到数据则放弃
            qWarning() << "UDP 未收到数据，停止重试，共尝试" << *udpAttemptCount << "次"; // 打印失败日志
            udpRetryTimer->stop(); // 停止定时器
            udpRetryTimer->deleteLater(); // 释放定时器
            m_UDPMatReceiver->stop(); // 停止内部重连定时器，避免继续发送握手日志
            return; // 结束本次回调
        }
        ++(*udpAttemptCount); // 增加尝试计数
        m_UDPMatReceiver->start("192.168.137.131", 9000); // 发起一次握手尝试等待数据
    });
    udpRetryTimer->start(); // 启动定时器开始尝试接收数据
}

 QVariantMap matchFunction::getMoveRotateDataMap() const{
    QVariantMap map;
    CopyItems_Move_Rotate_Data data = m_MoveRotateData; // 获取原有数据
    // 根据缩放比例调整偏移量
    if(m_scaleX != 0 || m_scaleY != 0){
        for(int i = 0; i < data.moveOffsets.size(); ++i){
            QPointF scaledPt(data.moveOffsets[i].x() * (1.0/m_scaleX), data.moveOffsets[i].y() * (1.0/m_scaleY));
            data.moveOffsets[i] = scaledPt;
        }
    }
    // 将 QVector 转换为 QVariantList
    QVariantList offsetsList;
    for(const auto& pt : data.moveOffsets) {
        offsetsList << QVariant::fromValue(pt);
    }

    QVariantList anglesList;
    for(const auto& angle : data.angles) {
        anglesList << angle;
    }

    map["moveOffsets"] = offsetsList;
    map["angles"] = anglesList;
    map["totalCounts"] = data.totalCounts;

    return map;
}

CopyItems_Move_Rotate_Data matchFunction::getMoveRotateData(){
    CopyItems_Move_Rotate_Data data = m_MoveRotateData; // 获取原有数据
    // 根据缩放比例调整偏移量
    if(m_scaleX != 0 || m_scaleY != 0){
        for(int i = 0; i < data.moveOffsets.size(); ++i){
            QPointF scaledPt(data.moveOffsets[i].x() * (1.0/m_scaleX), data.moveOffsets[i].y() * (1.0/m_scaleY));
            data.moveOffsets[i] = scaledPt;
        }
    }
    return data;
}

Match* newMatch() {
    return new matchFunction();
}

void matchFunction::ShowPath()
{
    scaleinitData();
    setMoveRotateData();

    if (m_ImageDisplayScene) {
        auto ms = static_cast<MatchScene*>(m_ImageDisplayScene); // 转换为 MatchScene
        if (!ms) {
            return;
        }
    }
        if(m_cale_initParam.combinedPath.isEmpty()){
        return ;
    }

    QPointF pathCenter(m_cale_initParam.unitedRect.x() + m_cale_initParam.unitedRect.width() / 2.0,
                       m_cale_initParam.unitedRect.y() + m_cale_initParam.unitedRect.height() / 2.0);
    QPointF pivotPoint = pathCenter; // 默认回退到路径中心
    
    if (!m_lastMatchResults.empty()) {
        int bestIdx = 0;
        double minDistance = std::numeric_limits<double>::max();
        for (int i = 0; i < static_cast<int>(m_lastMatchResults.size()); ++i) {
            double dx = m_lastMatchResults[i].center.x - pathCenter.x();
            double dy = m_lastMatchResults[i].center.y - pathCenter.y();
            double dist = dx*dx + dy*dy;
            if (dist < minDistance) {
                minDistance = dist;
                bestIdx = i;
            }
        }
        pivotPoint.setX(m_lastMatchResults[bestIdx].center.x); // 使用基准匹配的中心作为旋转轴心
        pivotPoint.setY(m_lastMatchResults[bestIdx].center.y); // 使用基准匹配的中心作为旋转轴心
    }

    m_drawPaths = copyPainterPathWithTransform(m_cale_initParam.combinedPath,
                                                    m_MoveRotateData.moveOffsets,
                                                    m_MoveRotateData.angles,
                                                    pivotPoint);                                          
    m_ImageDisplayScene->clear();
    for(int i = 1; i < m_drawPaths.size(); ++i){
        m_MoveRotateData.moveOffsets[i-1] = QPointF(m_drawPaths[i].boundingRect().center().x(),
                                                  m_drawPaths[i].boundingRect().center().y());
        qDebug().noquote() << QStringLiteral("路径数据: 位置=(%1, %2), 角度=%3°")
                                .arg(m_MoveRotateData.moveOffsets[i-1].x(), 0, 'f', 2)
                                .arg(m_MoveRotateData.moveOffsets[i-1].y(), 0, 'f', 2)
                                .arg(m_MoveRotateData.angles[i-1], 0, 'f', 2);
    }
    m_ImageDisplayScene->DrawTotalPath(m_drawPaths);
}

QVector<QPainterPath> matchFunction::copyPainterPathWithTransform(const QPainterPath& originalPath, const QVector<QPointF>& moveOffsets,
                                                   const QVector<double>& angles, QPointF pivotPoint)
{
    auto ms = qobject_cast<MatchScene*>(m_ImageDisplayScene);
    QVector<QPainterPath> newPaths;
    newPaths.append(originalPath); // 包含原始路径
    if (moveOffsets.size() != angles.size()) {
        qWarning() << "copyPainterPathWithTransform：偏移量数量(" << moveOffsets.size() 
                   << ")与角度数量(" << angles.size() << ")不一致！";
        return newPaths;
    }
    QTransform imgToScene;
    if (ms) {
        QPointF p0 = ms->imageToScenePoint(QPointF(0, 0));
        QPointF px = ms->imageToScenePoint(QPointF(1, 0));
        QPointF py = ms->imageToScenePoint(QPointF(0, 1));
        imgToScene = QTransform(px.x() - p0.x(), px.y() - p0.y(),
                                py.x() - p0.x(), py.y() - p0.y(),
                                p0.x(), p0.y());
    }

    QPainterPath scenePath = ms ? imgToScene.map(originalPath) : originalPath;

    // 计算场景坐标系下的旋转轴心
    QPointF pivotScene = ms ? ms->imageToScenePoint(pivotPoint) : pivotPoint;
    QPointF vecRef = ms ? ms->imageToScenePoint(QPointF(0,0)) : QPointF(0,0);

    for (int i = 0; i < moveOffsets.size(); ++i) {
        //const QPointF& offsetImg = moveOffsets[i]; // 这是 Image 坐标下的位移 vector
        QPointF offsetImg = QPointF();
        offsetImg.setX(moveOffsets[i].x());
        offsetImg.setY(moveOffsets[i].y());
        QPointF offsetScene = offsetImg;
        if (ms) {
            QPointF pOffset = ms->imageToScenePoint(offsetImg);
            offsetScene = pOffset - vecRef;
        }
        double angle = angles[i];
        QTransform transform;
        transform.translate(pivotScene.x() + offsetScene.x(), pivotScene.y() + offsetScene.y()); 
        transform.rotate(angle);
        transform.translate(-pivotScene.x(), -pivotScene.y()); 
        // 使用已经转换到场景坐标的 path
        QPainterPath newPath = transform.map(scenePath);
        newPaths.append(newPath);
    }
    return newPaths;
}

void matchFunction::scaleinitData()
{
    if(m_initParam.combinedPath.isEmpty()){
        return;
    }
    m_scaleX = m_initParam.img.width()/m_initParam.canvasSize.width();
    m_scaleY = m_initParam.img.height()/m_initParam.canvasSize.height();
    QTransform coordTransform;
    // 按坐标系的X、Y比例缩放
    coordTransform.scale(m_scaleX, m_scaleY);
    // 2. 将变换应用到原始路径，得到适配新坐标系的路径
    m_cale_initParam.combinedPath = coordTransform.map(m_initParam.combinedPath);
    m_cale_initParam.img = m_initParam.img;
    m_cale_initParam.canvasSize = m_initParam.canvasSize;
    m_cale_initParam.unitedRect.setX(m_initParam.unitedRect.x() *  m_scaleX);
    m_cale_initParam.unitedRect.setY(m_initParam.unitedRect.y() *  m_scaleY);
    m_cale_initParam.unitedRect.setWidth(m_initParam.unitedRect.width() *  m_scaleX);
    m_cale_initParam.unitedRect.setHeight(m_initParam.unitedRect.height() *  m_scaleY);
}

IMatchScene* matchFunction::getScene() const
{
    return m_ImageDisplayScene;
}

bool matchFunction::buildFindParams(FindMatchParams& params)
{
    m_FindMatchParams = params;
    return true;
}

bool matchFunction::setinitData(const initOrionVisionParam& para)
{
    m_initParam = para;
    cv::Mat OrionMat;

    if (!TIGER_BSVISION::qImage2cvImage(m_initParam.img, OrionMat) || OrionMat.empty()) {
        qDebug() << "当前图像转换失败，无法发送到模板匹配窗口";
        return false;
    }
    setcurrentImage(OrionMat);
    return true;
}
