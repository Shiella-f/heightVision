#include "HeightScene.h"

#include <QColor>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QPen>
#include <QGraphicsView>
#include <QEvent>
#include <QLineF>
#include <array>
#include <algorithm>
#include <cmath>


HeightScene::HeightScene(QObject *parent)
    : ImageSceneBase(parent)
{
}

void HeightScene::updateDragMode() const
{
    if (!m_view) {
        return;
    }
    const bool enableDrag = hasImage() && !m_roiSelectionEnabled && m_roiAdjustMode == RoiAdjustMode::None;
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setHorizontalScrollBarPolicy(enableDrag ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(enableDrag ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    updateCursorState();
}

void HeightScene::updateCursorState() const
{
    if (!m_view) {
        return;
    }

    if (m_roiSelectionEnabled && hasImage()) {
        m_view->setCursor(Qt::CrossCursor);
        return;
    }

    auto effectiveMode = m_roiAdjustMode != RoiAdjustMode::None ? m_roiAdjustMode : m_roiHoverMode;
    switch (effectiveMode) {
    case RoiAdjustMode::Move:
        m_view->setCursor(Qt::SizeAllCursor);
        return;
    case RoiAdjustMode::Left:
    case RoiAdjustMode::Right:
        m_view->setCursor(Qt::SizeHorCursor);
        return;
    case RoiAdjustMode::Top:
    case RoiAdjustMode::Bottom:
        m_view->setCursor(Qt::SizeVerCursor);
        return;
    case RoiAdjustMode::TopLeft:
    case RoiAdjustMode::BottomRight:
        m_view->setCursor(Qt::SizeFDiagCursor);
        return;
    case RoiAdjustMode::TopRight:
    case RoiAdjustMode::BottomLeft:
        m_view->setCursor(Qt::SizeBDiagCursor);
        return;
    default:
        break;
    }

    m_view->unsetCursor();
}

void HeightScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_roiSelectionEnabled && m_isCollectRoi && hasImage() && event->button() == Qt::LeftButton) {
        QRectF sceneRect = currentSceneRoiRect();
        const RoiAdjustMode hit = hitTestRoiHandle(event->scenePos(), sceneRect);
        if (hit != RoiAdjustMode::None) {
            m_roiAdjustMode = hit;
            m_roiAdjustInitialSceneRect = sceneRect;
            m_roiAdjustStartScenePos = event->scenePos();
            updateDragMode();
            event->accept();
            return;
        }
    }

    if (m_roiSelectionEnabled && hasImage() && event->button() == Qt::LeftButton) { // 在 ROI 模式下按下左键开始框选
        ensureRoiItem(); // 确保 ROI 可视化矩形已创建
        m_roiDragging = true; // 标记进入拖拽状态
        m_roiStartScene = event->scenePos(); // 记录拖拽起点（场景坐标）
        updateRoiItem(QRectF(m_roiStartScene, QSizeF()), true); // 初始化显示一个零尺寸的 ROI 框
        m_roiHoverMode = RoiAdjustMode::None;
        updateCursorState();
        event->accept(); // 拦截事件避免继续传递
        return; // 直接返回，不再执行默认逻辑
    }

    QGraphicsScene::mousePressEvent(event);
}

void HeightScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    updateRoiHoverState(event->scenePos());

    if (m_roiAdjustMode != RoiAdjustMode::None) {
        updateRoiAdjusting(event->scenePos());
        event->accept();
        return;
    }

    if (m_roiDragging && m_roiSelectionEnabled) { // 拖拽过程中实时更新 ROI 框
        QRectF sceneRect(m_roiStartScene, event->scenePos()); // 构造当前拖拽矩形
        updateRoiItem(sceneRect.normalized(), true); // 规范化矩形并刷新显示
        event->accept(); // 吃掉事件以避免默认行为
        return; // 不继续向下传递
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void HeightScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_roiAdjustMode != RoiAdjustMode::None && event->button() == Qt::LeftButton) {
        updateRoiAdjusting(event->scenePos());
        m_roiAdjustMode = RoiAdjustMode::None;
        updateDragMode();
        QRectF newSceneRect = currentSceneRoiRect();
        m_roiHoverMode = hitTestRoiHandle(event->scenePos(), newSceneRect);
        updateCursorState();
        event->accept();
        return;
    }

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
            m_roiArea = QRectF(); 
            updateRoiItem(QRectF(), false); // 隐藏显示矩形
        }
        updateDragMode();
        QRectF newSceneRect = currentSceneRoiRect();
        m_roiHoverMode = hitTestRoiHandle(event->scenePos(), newSceneRect);
        updateCursorState();
        event->accept(); 
        return; 
    }

    QGraphicsScene::mouseReleaseEvent(event);
}

void HeightScene::setRoiArea(const QRectF &roi)
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

bool HeightScene::getRoiArea(QRectF &roi) const
{
    if (!m_isCollectRoi) {
        return false;
    }
    roi = m_roiArea;
    return true;
}

void HeightScene::enableRoiSelection(bool enable)
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

void HeightScene::setRoiSelectionEnabled(bool enabled)
{
    enableRoiSelection(enabled); // 复用内部逻辑保持行为一致
}

void HeightScene::clearRoi()
{
    m_isCollectRoi = false; // 清除 ROI 有效标记
    m_roiArea = QRectF(); // 重置 ROI 数据
    m_roiDragging = false; // 退出拖拽状态
    m_roiAdjustMode = RoiAdjustMode::None; // 停止 ROI 调整
    if (m_roiRectItem) {
        m_roiRectItem->setVisible(false); // 隐藏 ROI 显示矩形
    }
    for (auto *handle : m_roiHandleItems) {
        if (handle) {
            handle->setVisible(false);
        }
    }
    m_roiHoverMode = RoiAdjustMode::None;
    updateCursorState();
}

void HeightScene::ensureRoiItem()
{
    if (!m_roiRectItem) { // 若尚未创建 ROI 图形项
        QPen pen(QColor(255, 165, 0)); // 构造橙色描边
        pen.setWidthF(1.2); // 设置线宽
        pen.setStyle(Qt::DashLine); // 使用虚线样式
        m_roiRectItem = addRect(QRectF(), pen, QColor(255, 165, 0, 40)); // 创建带半透明填充的矩形
        m_roiRectItem->setZValue(3.0); // 提升层级避免被遮挡
        m_roiRectItem->setFlag(QGraphicsItem::ItemIsSelectable, false); // 禁止用户单独选中
        m_roiRectItem->setFlag(QGraphicsItem::ItemIsMovable, false); // 禁止直接拖动矩形
        m_roiRectItem->setAcceptedMouseButtons(Qt::NoButton);
        m_roiRectItem->setAcceptHoverEvents(false);
    }

    const QColor handleColor(255, 215, 0);
    const QColor handleFill(255, 215, 0, 180);
    for (auto &handle : m_roiHandleItems) {
        if (!handle) {
            QPen handlePen(handleColor);
            handlePen.setWidthF(1.0);
            handle = addRect(QRectF(), handlePen, handleFill);
            handle->setZValue(3.5);
            handle->setFlag(QGraphicsItem::ItemIsSelectable, false);
            handle->setFlag(QGraphicsItem::ItemIsMovable, false);
            handle->setAcceptedMouseButtons(Qt::NoButton);
            handle->setAcceptHoverEvents(false);
            handle->setVisible(false);
        }
    }
}

void HeightScene::updateRoiItem(const QRectF &sceneRect, bool visible)
{
    ensureRoiItem(); // 确保矩形已创建
    m_roiRectItem->setRect(sceneRect); // 更新矩形范围
    const bool show = visible && !sceneRect.isNull() && sceneRect.width() > 0.0 && sceneRect.height() > 0.0;
    m_roiRectItem->setVisible(show); // 根据条件控制显示

    const double handleSize = 10.0;
    const double half = handleSize / 2.0;
    const std::array<QPointF, 4> corners{
        sceneRect.topLeft(),
        sceneRect.topRight(),
        sceneRect.bottomLeft(),
        sceneRect.bottomRight()
    };

    for (size_t idx = 0; idx < m_roiHandleItems.size(); ++idx) {
        auto *handle = m_roiHandleItems[idx];
        if (!handle) {
            continue;
        }
        if (!show) {
            handle->setVisible(false);
            continue;
        }
        const QPointF &corner = corners[idx];
        handle->setRect(QRectF(corner.x() - half, corner.y() - half, handleSize, handleSize));
        handle->setVisible(true);
    }

    if (!show) {
        m_roiHoverMode = RoiAdjustMode::None;
        updateCursorState();
    }
}

QRectF HeightScene::clampToImage(const QRectF &imageRect) const
{
    QRectF normalized = imageRect.normalized(); // 规范化矩形方向
    if (!m_sourceImageSize.isValid() || normalized.isNull()) {
        return normalized; // 若图像尺寸无效则直接返回
    }

    QRectF bounds(QPointF(0.0, 0.0), m_sourceImageSize); // 以原图大小构造边界
    QRectF clamped = normalized.intersected(bounds); // 与边界求交确保 ROI 在图像内
    return clamped; // 返回限制后的矩形
}

QRectF HeightScene::currentSceneRoiRect() const
{
    if (!m_isCollectRoi || !hasImage()) {
        return QRectF();
    }

    QPointF topLeft = mapImageToScene(m_roiArea.topLeft());
    QPointF bottomRight = mapImageToScene(m_roiArea.bottomRight());
    return QRectF(topLeft, bottomRight).normalized();
}

void HeightScene::applySceneRoiRect(const QRectF &sceneRect)
{
    if (!hasImage() || sceneRect.isNull()) {
        return;
    }
    QPointF imageTopLeft = mapSceneToImage(sceneRect.topLeft());
    QPointF imageBottomRight = mapSceneToImage(sceneRect.bottomRight());
    QRectF imageRect(imageTopLeft, imageBottomRight);
    setRoiArea(imageRect);
}

HeightScene::RoiAdjustMode HeightScene::hitTestRoiHandle(const QPointF &scenePos, const QRectF &sceneRect) const
{
    if (sceneRect.isNull()) {
        return RoiAdjustMode::None;
    }

    const double handleRadius = 8.0;
    const double edgeTolerance = 5.0;
    const double minInteriorPadding = handleRadius + 1.0;
    const QPointF topLeft = sceneRect.topLeft();
    const QPointF topRight = sceneRect.topRight();
    const QPointF bottomLeft = sceneRect.bottomLeft();
    const QPointF bottomRight = sceneRect.bottomRight();

    const auto within = [&](const QPointF &corner) {
        return QLineF(scenePos, corner).length() <= handleRadius;
    };

    if (within(topLeft)) {
        return RoiAdjustMode::TopLeft;
    }
    if (within(topRight)) {
        return RoiAdjustMode::TopRight;
    }
    if (within(bottomLeft)) {
        return RoiAdjustMode::BottomLeft;
    }
    if (within(bottomRight)) {
        return RoiAdjustMode::BottomRight;
    }

    const bool withinVertical = scenePos.y() >= topLeft.y() + minInteriorPadding && scenePos.y() <= bottomLeft.y() - minInteriorPadding;
    if (withinVertical) {
        if (std::abs(scenePos.x() - topLeft.x()) <= edgeTolerance) {
            return RoiAdjustMode::Left;
        }
        if (std::abs(scenePos.x() - topRight.x()) <= edgeTolerance) {
            return RoiAdjustMode::Right;
        }
    }

    const bool withinHorizontal = scenePos.x() >= topLeft.x() + minInteriorPadding && scenePos.x() <= topRight.x() - minInteriorPadding;
    if (withinHorizontal) {
        if (std::abs(scenePos.y() - topLeft.y()) <= edgeTolerance) {
            return RoiAdjustMode::Top;
        }
        if (std::abs(scenePos.y() - bottomLeft.y()) <= edgeTolerance) {
            return RoiAdjustMode::Bottom;
        }
    }

    QRectF interiorRect = sceneRect.adjusted(edgeTolerance, edgeTolerance, -edgeTolerance, -edgeTolerance);
    if (!interiorRect.isNull() && interiorRect.contains(scenePos)) {
        return RoiAdjustMode::Move;
    }
    return RoiAdjustMode::None;
}

void HeightScene::updateRoiAdjusting(const QPointF &scenePos)
{
    if (m_roiAdjustMode == RoiAdjustMode::None) {
        return;
    }

    QRectF rect = m_roiAdjustInitialSceneRect;
    const QPointF delta = scenePos - m_roiAdjustStartScenePos;

    switch (m_roiAdjustMode) {
    case RoiAdjustMode::Move:
        rect.translate(delta);
        break;
    case RoiAdjustMode::TopLeft:
        rect.setTopLeft(m_roiAdjustInitialSceneRect.topLeft() + delta);
        break;
    case RoiAdjustMode::TopRight:
        rect.setTopRight(m_roiAdjustInitialSceneRect.topRight() + delta);
        break;
    case RoiAdjustMode::BottomLeft:
        rect.setBottomLeft(m_roiAdjustInitialSceneRect.bottomLeft() + delta);
        break;
    case RoiAdjustMode::BottomRight:
        rect.setBottomRight(m_roiAdjustInitialSceneRect.bottomRight() + delta);
        break;
    case RoiAdjustMode::Left:
        rect.setLeft(m_roiAdjustInitialSceneRect.left() + delta.x());
        break;
    case RoiAdjustMode::Right:
        rect.setRight(m_roiAdjustInitialSceneRect.right() + delta.x());
        break;
    case RoiAdjustMode::Top:
        rect.setTop(m_roiAdjustInitialSceneRect.top() + delta.y());
        break;
    case RoiAdjustMode::Bottom:
        rect.setBottom(m_roiAdjustInitialSceneRect.bottom() + delta.y());
        break;
    case RoiAdjustMode::None:
        break;
    }

    rect = rect.normalized();
    if (rect.width() < 2.0 || rect.height() < 2.0) {
        return;
    }

    applySceneRoiRect(rect);
    m_roiAdjustInitialSceneRect = currentSceneRoiRect();
    m_roiAdjustStartScenePos = scenePos;
}

void HeightScene::updateRoiHoverState(const QPointF &scenePos)
{
    const bool roiSelectable = !m_roiSelectionEnabled && m_isCollectRoi && hasImage();
    const bool allowHover = roiSelectable && m_roiAdjustMode == RoiAdjustMode::None && !m_roiDragging;

    if (allowHover) {
        QRectF sceneRect = currentSceneRoiRect();
        RoiAdjustMode hover = hitTestRoiHandle(scenePos, sceneRect);
        if (hover != m_roiHoverMode) {
            m_roiHoverMode = hover;
            updateCursorState();
        }
    } else if (m_roiHoverMode != RoiAdjustMode::None) {
        m_roiHoverMode = RoiAdjustMode::None;
        updateCursorState();
    }
}

void HeightScene::onViewAttached(QGraphicsView *newView)
{
    ImageSceneBase::onViewAttached(newView);
    updateDragMode();
}

void HeightScene::onImageCleared()
{
    ImageSceneBase::onImageCleared();
    m_roiSelectionEnabled = false;
    clearRoi();
    updateDragMode();
}

void HeightScene::onImageUpdated()
{
    ImageSceneBase::onImageUpdated();
    if (m_isCollectRoi && hasImage() && !m_roiArea.isNull()) {
        QRectF sceneRect(mapImageToScene(m_roiArea.topLeft()),
                         mapImageToScene(m_roiArea.bottomRight()));
        updateRoiItem(sceneRect.normalized(), true);
    }
    updateDragMode();
}

bool HeightScene::event(QEvent *event)
{
    if (event) {
        const QEvent::Type type = event->type();
        if ((type == QEvent::Leave /*|| type == QEvent::GraphicsSceneLeave*/) && m_roiHoverMode != RoiAdjustMode::None) {
            m_roiHoverMode = RoiAdjustMode::None;
            updateCursorState();
        }
    }

    return ImageSceneBase::event(event);
}