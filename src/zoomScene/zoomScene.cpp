#include "zoomScene.h"

#include <QAction>
#include <QFrame>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QPen>
#include <QGraphicsView>
#include <QLineF>
#include <QMenu>
#include <QPainter>
#include <QTransform>
#include <QtMath>
#include <QtGlobal>
#include <algorithm>
#include <utility>


namespace {
QRectF centeredRect(const QSizeF &size)
{
    const QPointF topLeft(-size.width() / 2.0, -size.height() / 2.0);
    return QRectF(topLeft, size);
}
}

ZoomScene::ZoomScene(QObject *parent)
    : QGraphicsScene(parent),
      m_view(nullptr),
      m_pixmapItem(nullptr),
      m_placeholderItem(nullptr),
      m_resetAction(new QAction(tr("恢复原图"), this)),
      m_scale(1.0),
      m_minScale(0.1),
      m_maxScale(5.0),
      m_hasImage(false),
      m_sourceImageSize()
{
    connect(m_resetAction, &QAction::triggered, this, &ZoomScene::resetImage);
}

void ZoomScene::attachView(QGraphicsView *view)
{
    if (m_view == view) {
        return;
    }

    if (m_view) {
        m_view->setScene(nullptr);
    }

    m_view = view;
    if (m_view) {
        m_view->setScene(this);
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform, true);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setInteractive(true);
    m_view->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setAlignment(Qt::AlignCenter);
    m_view->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
        updateDragMode();

        connect(m_view, &QObject::destroyed, this, [this]() { m_view = nullptr; });
    }

    resetImage();
}

void ZoomScene::setOriginalPixmap(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        clearImage();
        return;
    }

    if (!m_pixmapItem) {
        m_pixmapItem = addPixmap(pixmap);
        m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);
    } else {
        m_pixmapItem->setPixmap(pixmap);
        m_pixmapItem->setVisible(true);
    }

    if (m_placeholderItem) {
        m_placeholderItem->setVisible(false);
    }

    m_hasImage = true;
    setSceneRect(m_pixmapItem->boundingRect());
    m_sourceImageSize = pixmap.size();
    updateDragMode();
    resetImage();

    if (m_isCollectRoi && !m_roiArea.isNull()) { // 如果已有有效 ROI，则在重新加载时恢复显示
        QRectF sceneRect(mapImageToScene(m_roiArea.topLeft()), // 将 ROI 左上角从图像坐标映射到场景坐标
                         mapImageToScene(m_roiArea.bottomRight())); // 将 ROI 右下角从图像坐标映射到场景坐标
        updateRoiItem(sceneRect.normalized(), true); // 更新 ROI 可视化矩形并设为可见
    }
}

void ZoomScene::showPlaceholder(const QString &text)
{
    ensurePlaceholderItem();

    QFont font = m_placeholderItem->font();
    font.setPointSize(std::max(10, font.pointSize()));
    m_placeholderItem->setFont(font);
    m_placeholderItem->setDefaultTextColor(QColor(160, 160, 160));
    m_placeholderItem->setPlainText(text);

    const QFontMetricsF metrics(font);
    QSizeF textSize = metrics.size(Qt::TextSingleLine, text);
    if (textSize.isEmpty()) {
        textSize = QSizeF(60.0, 20.0);
    }

    const QRectF rect = centeredRect(textSize + QSizeF(20.0, 12.0));
    setSceneRect(rect);
    m_placeholderItem->setPos(rect.topLeft() + QPointF((rect.width() - textSize.width()) / 2.0,
                                                       (rect.height() - textSize.height()) / 2.0));

    m_hasImage = false;
    if (m_pixmapItem) {
        m_pixmapItem->setVisible(false);
    }
    m_sourceImageSize = QSizeF();

    if (m_view) {
        m_view->setTransform(QTransform());
        m_view->centerOn(m_placeholderItem);
    }

    updateDragMode();
}

void ZoomScene::clearImage()
{
    m_hasImage = false; // 标记当前无图像
    m_scale = 1.0; // 重置缩放倍数
    m_roiSelectionEnabled = false; // 关闭 ROI 选取模式
    if (m_pixmapItem) {
        m_pixmapItem->setVisible(false);
        m_pixmapItem->setPixmap(QPixmap());
    }
    if (m_view) {
        m_view->setTransform(QTransform());
        m_view->centerOn(0.0, 0.0);
    }
    m_sourceImageSize = QSizeF(); // 清空原始图像尺寸
    clearRoi(); // 清除已有的 ROI 状态
    updateDragMode();
}

void ZoomScene::resetImage()
{
    if (!m_view || !m_pixmapItem || !m_hasImage) {
        return;
    }

    m_scale = 1.0;
    m_view->setTransform(QTransform());
    m_view->centerOn(m_pixmapItem);
}

QSize ZoomScene::availableSize() const
{
    if (m_view) {
        const QSize size = m_view->viewport()->size();
        if (size.width() > 0 && size.height() > 0) {
            return size;
        }
    }
    return QSize(320, 240);
}

bool ZoomScene::hasImage() const
{
    return m_hasImage && m_pixmapItem && !m_pixmapItem->pixmap().isNull();
}

void ZoomScene::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (!m_view || !hasImage()) {
        event->ignore();
        return;
    }

    const int delta = event->delta();
    if (delta == 0) {
        event->accept();
        return;
    }

    const double factor = delta > 0 ? 1.1 : 0.9;
    applyScale(factor);
    event->accept();
}

void ZoomScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (!hasImage()) {
        event->ignore();
        return;
    }

    QMenu menu;
    menu.addAction(m_resetAction);
    menu.exec(event->screenPos());
    event->accept();
}

void ZoomScene::ensurePlaceholderItem()
{
    if (!m_placeholderItem) {
        m_placeholderItem = addText(QString());
        m_placeholderItem->setZValue(1.0);
        m_placeholderItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_placeholderItem->setFlag(QGraphicsItem::ItemIsMovable, false);
    }
}

void ZoomScene::updateDragMode() const
{
    if (!m_view) {
        return;
    }
    const bool enableDrag = hasImage() && !m_roiSelectionEnabled; // 仅在存在图像且未进入 ROI 模式时允许拖拽
    // ScrollHandDrag 模式会在按住鼠标左键移动时平移视图，看起来就像拖动图片本身。
    m_view->setDragMode(enableDrag ? QGraphicsView::ScrollHandDrag : QGraphicsView::NoDrag);
    // 同步滚动条的可见性与鼠标光标，给用户明确的拖拽/静止状态反馈。
    m_view->setHorizontalScrollBarPolicy(enableDrag ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(enableDrag ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setCursor(enableDrag ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void ZoomScene::applyScale(double factor)
{
    if (!m_view || qFuzzyIsNull(factor - 1.0)) {
        return;
    }

    const double newScale = qBound(m_minScale, m_scale * factor, m_maxScale);
    const double actualFactor = newScale / m_scale;
    if (qFuzzyIsNull(actualFactor - 1.0)) {
        return;
    }

    m_view->scale(actualFactor, actualFactor);
    m_scale = newScale;
}

void ZoomScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_roiSelectionEnabled && hasImage() && event->button() == Qt::LeftButton) { // 在 ROI 模式下按下左键开始框选
        ensureRoiItem(); // 确保 ROI 可视化矩形已创建
        m_roiDragging = true; // 标记进入拖拽状态
        m_roiStartScene = event->scenePos(); // 记录拖拽起点（场景坐标）
        updateRoiItem(QRectF(m_roiStartScene, QSizeF()), true); // 初始化显示一个零尺寸的 ROI 框
        event->accept(); // 拦截事件避免继续传递
        return; // 直接返回，不再执行默认逻辑
    }

    QGraphicsScene::mousePressEvent(event);
}

void ZoomScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_roiDragging && m_roiSelectionEnabled) { // 拖拽过程中实时更新 ROI 框
        QRectF sceneRect(m_roiStartScene, event->scenePos()); // 构造当前拖拽矩形
        updateRoiItem(sceneRect.normalized(), true); // 规范化矩形并刷新显示
        event->accept(); // 吃掉事件以避免默认行为
        return; // 不继续向下传递
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void ZoomScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_roiDragging && m_roiSelectionEnabled && event->button() == Qt::LeftButton) { // 松开左键时结束 ROI 拾取
        m_roiDragging = false; // 结束拖拽状态
        m_roiSelectionEnabled = false; // 退出 ROI 选取模式
        QRectF sceneRect(m_roiStartScene, event->scenePos()); // 根据起止点得到场景矩形
        sceneRect = sceneRect.normalized(); // 规范化矩形确保左上右下顺序
        if (sceneRect.width() > 1.0 && sceneRect.height() > 1.0) { // 过滤过小的框选
            QPointF imageTopLeft = mapSceneToImage(sceneRect.topLeft()); // 转换左上角到图像坐标
            QPointF imageBottomRight = mapSceneToImage(sceneRect.bottomRight()); // 转换右下角到图像坐标
            QRectF imageRect(imageTopLeft, imageBottomRight); // 构造图像坐标系矩形
            imageRect = clampToImage(imageRect.normalized()); // 限制 ROI 在图像范围内
            m_roiArea = imageRect; // 保存最终 ROI
            m_isCollectRoi = imageRect.width() > 0.0 && imageRect.height() > 0.0; // 标记是否选取成功
            if (m_isCollectRoi) { // 若选取有效则刷新显示
                QRectF displayRect(mapImageToScene(m_roiArea.topLeft()), // 将 ROI 左上角映射回场景
                                   mapImageToScene(m_roiArea.bottomRight())); // 将 ROI 右下角映射回场景
                updateRoiItem(displayRect.normalized(), true); // 用图像范围限制后的矩形更新显示
            } else {
                updateRoiItem(QRectF(), false); // 无效时隐藏 ROI
            }
        } else {
            m_isCollectRoi = false; // 框选太小视为无效
            m_roiArea = QRectF(); // 清空 ROI
            updateRoiItem(QRectF(), false); // 隐藏显示矩形
        }
        event->accept(); // 吃掉释放事件
        return; // 结束处理
    }

    QGraphicsScene::mouseReleaseEvent(event);
}

void ZoomScene::setSourceImageSize(const QSize &size)
{
    m_sourceImageSize = size;
}

QPointF ZoomScene::mapSceneToImage(const QPointF &scenePoint) const
{
    if (!m_pixmapItem) {
        return scenePoint;
    }

    const QPointF localPoint = m_pixmapItem->mapFromScene(scenePoint);
    const QSizeF pixSize = m_pixmapItem->pixmap().size();
    if (pixSize.isEmpty()) {
        return localPoint;
    }

    if (!m_sourceImageSize.isValid()) {
        return localPoint;
    }

    const double scaleX = m_sourceImageSize.width() / pixSize.width();
    const double scaleY = m_sourceImageSize.height() / pixSize.height();
    return QPointF(localPoint.x() * scaleX, localPoint.y() * scaleY);
}

void ZoomScene::setRoiArea(const QRectF &roi)
{
    QRectF clamped = clampToImage(roi); // 将传入的 ROI 限制到图像范围内
    m_roiArea = clamped; // 保存最终的 ROI 转换结果
    m_isCollectRoi = clamped.width() > 0.0 && clamped.height() > 0.0; // 判断 ROI 是否有效
    if (hasImage() && m_isCollectRoi) { // 若当前存在图像且 ROI 合法
        QRectF sceneRect(mapImageToScene(clamped.topLeft()), // 将左上角映射到场景坐标
                         mapImageToScene(clamped.bottomRight())); // 将右下角映射到场景坐标
        updateRoiItem(sceneRect.normalized(), true); // 显示 ROI 矩形
    } else {
        updateRoiItem(QRectF(), false); // 否则隐藏 ROI
    }
}

bool ZoomScene::getRoiArea(QRectF &roi) const
{
    if (!m_isCollectRoi) {
        return false;
    }
    roi = m_roiArea;
    return true;
}

void ZoomScene::enableRoiSelection(bool enable)
{
    if (!hasImage()) {
        enable = false; // 若没有图像则强制关闭 ROI 模式
    }
    m_roiSelectionEnabled = enable; // 记录当前 ROI 模式状态
    m_roiDragging = false; // 重置拖拽标记
    if (m_isCollectRoi && hasImage()) { // 若已有 ROI 且图像存在
        QRectF sceneRect(mapImageToScene(m_roiArea.topLeft()), // 将 ROI 左上角映射至场景
                         mapImageToScene(m_roiArea.bottomRight())); // 将 ROI 右下角映射至场景
        updateRoiItem(sceneRect.normalized(), true); // 继续显示 ROI
    } else if (!m_isCollectRoi) {
        updateRoiItem(QRectF(), false); // 无 ROI 时隐藏矩形
    }
    updateDragMode(); // 切换拖拽模式以防冲突
}

void ZoomScene::setRoiSelectionEnabled(bool enabled)
{
    enableRoiSelection(enabled); // 复用内部逻辑保持行为一致
}

bool ZoomScene::isRoiSelectionEnabled() const
{
    return m_roiSelectionEnabled;
}

void ZoomScene::clearRoi()
{
    m_isCollectRoi = false; // 清除 ROI 有效标记
    m_roiArea = QRectF(); // 重置 ROI 数据
    m_roiDragging = false; // 退出拖拽状态
    if (m_roiRectItem) {
        m_roiRectItem->setVisible(false); // 隐藏 ROI 显示矩形
    }
}

QPointF ZoomScene::mapImageToScene(const QPointF &imagePoint) const
{
    if (!m_pixmapItem) {
        return imagePoint; // 若无图像项则直接返回原坐标
    }

    const QSizeF pixSize = m_pixmapItem->pixmap().size(); // 获取当前显示图元的像素尺寸
    if (pixSize.isEmpty() || !m_sourceImageSize.isValid()) {
        return m_pixmapItem->mapToScene(imagePoint); // 若无法计算缩放则直接映射
    }

    const double scaleX = pixSize.width() / m_sourceImageSize.width(); // 计算 X 轴缩放比例
    const double scaleY = pixSize.height() / m_sourceImageSize.height(); // 计算 Y 轴缩放比例
    QPointF local(imagePoint.x() * scaleX, imagePoint.y() * scaleY); // 将图像坐标缩放到图元坐标
    return m_pixmapItem->mapToScene(local); // 转为场景坐标
}

void ZoomScene::ensureRoiItem()
{
    if (!m_roiRectItem) { // 若尚未创建 ROI 图形项
        QPen pen(QColor(255, 165, 0)); // 构造橙色描边
        pen.setWidthF(1.2); // 设置线宽
        pen.setStyle(Qt::DashLine); // 使用虚线样式
        m_roiRectItem = addRect(QRectF(), pen, QColor(255, 165, 0, 40)); // 创建带半透明填充的矩形
        m_roiRectItem->setZValue(3.0); // 提升层级避免被遮挡
        m_roiRectItem->setFlag(QGraphicsItem::ItemIsSelectable, false); // 禁止用户单独选中
        m_roiRectItem->setFlag(QGraphicsItem::ItemIsMovable, false); // 禁止直接拖动矩形
    }
}

void ZoomScene::updateRoiItem(const QRectF &sceneRect, bool visible)
{
    ensureRoiItem(); // 确保矩形已创建
    m_roiRectItem->setRect(sceneRect); // 更新矩形范围
    m_roiRectItem->setVisible(visible && !sceneRect.isNull() && sceneRect.width() > 0.0 && sceneRect.height() > 0.0); // 根据条件控制显示
}

QRectF ZoomScene::clampToImage(const QRectF &imageRect) const
{
    QRectF normalized = imageRect.normalized(); // 规范化矩形方向
    if (!m_sourceImageSize.isValid() || normalized.isNull()) {
        return normalized; // 若图像尺寸无效则直接返回
    }

    QRectF bounds(QPointF(0.0, 0.0), m_sourceImageSize); // 以原图大小构造边界
    QRectF clamped = normalized.intersected(bounds); // 与边界求交确保 ROI 在图像内
    return clamped; // 返回限制后的矩形
}