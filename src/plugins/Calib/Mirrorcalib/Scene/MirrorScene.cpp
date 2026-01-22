#include "MirrorScene.h"
#include <QColor>
#include <QGraphicsView>


MirrorScene::MirrorScene(QObject *parent)
    : ImageSceneBase(parent)
{
    
}

void MirrorScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) 
    {
        const QPointF imgPt = mapSceneToImage(event->scenePos());
        m_currentClickPos = imgPt;
        emit currentPointChanged(m_currentClickPos);
    }
    if(event->button() == Qt::LeftButton && !m_roiSelectionEnabled && hasImage() && m_isCollectRoi)
    {
        QRectF sceneRoiRect = currentSceneRoiRect();  
        const RoiAdjustMode hitMode = hitTestRoiHandle(event->scenePos(), sceneRoiRect);
        if(hitMode != RoiAdjustMode::None)
        {
            m_roiAdjustMode = hitMode;
            m_roiAdjustInitialSceneRect = sceneRoiRect;
            m_roiAdjustStartScenePos = event->scenePos();
            updateDragMode();
            event->accept();
            return;
        }
    }
    if(event->button() == Qt::LeftButton && m_roiSelectionEnabled && hasImage())
    {
        ensureRoiItem();
        m_roiDragging = true;
        m_roiStartScene = event->scenePos();
        updateRoiItem(QRectF(m_roiStartScene, QSizeF()), true);
        m_roiHoverMode = RoiAdjustMode::None;
        updateCursorState();
        event->accept();
        return;
    }

    QGraphicsScene::mousePressEvent(event);
}

void MirrorScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    updateRoiHoverState(event->scenePos());
    if(m_roiAdjustMode != RoiAdjustMode::None)
    {
        updateRoiAdjusting(event->scenePos());
        event->accept();
        return;
    }
    if(m_roiDragging && m_roiSelectionEnabled)
    {
        QRectF sceneRect(m_roiStartScene, event->scenePos());
        updateRoiItem(sceneRect.normalized(), true);
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void MirrorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if(m_roiAdjustMode != RoiAdjustMode::None && event->button() == Qt::LeftButton)
    {
        updateRoiAdjusting(event->scenePos());
        m_roiAdjustMode = RoiAdjustMode::None;
        updateDragMode();
        QRectF newSceneRect = currentSceneRoiRect();
        m_roiHoverMode = hitTestRoiHandle(event->scenePos(), newSceneRect);
        updateCursorState();
        event->accept();
        return;
    }
    if(m_roiDragging && m_roiSelectionEnabled && event->button() == Qt::LeftButton)
    {
        m_roiDragging = false; // 结束拖拽状态
        m_roiSelectionEnabled = false; // 退出 ROI 选取模式
        QRectF sceneRect(m_roiStartScene, event->scenePos());
        sceneRect = sceneRect.normalized();
        if(sceneRect.width()>1.0&&sceneRect.height()>1.0)
        {
            QPointF ImgTopLeft = mapSceneToImage(sceneRect.topLeft());;
            QPointF ImgBottomRight = mapSceneToImage(sceneRect.bottomRight());
            QRectF imgRoiRect(ImgTopLeft, ImgBottomRight);
            imgRoiRect = clampToImage(imgRoiRect);
            m_roiArea = imgRoiRect;
            m_isCollectRoi = imgRoiRect.width() > 0.0 && imgRoiRect.height() > 0.0;
            if(m_isCollectRoi)
            {
                QRectF sceneRoiRect(mapImageToScene(m_roiArea.topLeft()), mapImageToScene(m_roiArea.bottomRight()));
                updateRoiItem(sceneRoiRect.normalized(), true);
            }else{updateRoiItem(QRectF(), false);}
        }else
        {
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

void MirrorScene::ensureRoiItem()
{
    if(m_roiRectItem == nullptr)
    {
        QPen pen(QColor(255,165,0)); 
        pen.setWidth(1.2);
        pen.setStyle(Qt::DashLine);
        m_roiRectItem = addRect(QRectF(), pen, QColor(255,165,1,40));
        m_roiRectItem->setZValue(3);
        m_roiRectItem->setFlag(QGraphicsItem::ItemIsMovable, false);
        m_roiRectItem->setFlag(QGraphicsItem::ItemIsSelectable, false);  
        m_roiRectItem->setAcceptedMouseButtons(Qt::NoButton);
        m_roiRectItem->setAcceptHoverEvents(false);
    }

    const QColor handleColor(255, 165, 0);
    const QColor handleFillColor(255, 215, 0, 180);
    for(auto &handle : m_roiHandleItems)
    {
        if(handle == nullptr)
        {
            handle = addRect(QRectF(), QPen(handleColor), QBrush(handleFillColor));
            handle->setZValue(4);
            handle->setFlag(QGraphicsItem::ItemIsMovable, false);
            handle->setFlag(QGraphicsItem::ItemIsSelectable, false);  
            handle->setAcceptedMouseButtons(Qt::NoButton);
            handle->setAcceptHoverEvents(false);
        }
    }
}

void MirrorScene::updateRoiItem(const QRectF &sceneRect, bool visible)
{
    ensureRoiItem();
    m_roiRectItem->setRect(sceneRect);
    const bool show = visible && !sceneRect.isNull() && sceneRect.width() > 0.0 && sceneRect.height() > 0.0;
    m_roiRectItem->setVisible(show);

    const double handleSize = m_sourceImageSize.width()/900*10;
    const double half = handleSize / 2.0;
    const std::array<QPointF, 4> corners{
        sceneRect.topLeft(),
        sceneRect.topRight(),
        sceneRect.bottomRight(),
        sceneRect.bottomLeft()
    };
    
    for(size_t idx = 0; idx < m_roiHandleItems.size(); ++idx)
    {
        auto* handle  = m_roiHandleItems[idx];
        if(!handle)
        {
            continue;
        }
        if(!show)
        {
            handle->setVisible(false);
            continue;
        }
        const QPointF &corner = corners[idx];
        handle->setRect(QRectF(corner.x() - half, corner.y() - half, handleSize, handleSize));
        handle->setVisible(true);
    }

    if(!show)
    {
        m_roiHoverMode = RoiAdjustMode::None;
        updateCursorState();
    }
}

void MirrorScene::updateRoiHoverState(const QPointF &scenePoint)
{
    const bool roiSelectable = !m_roiSelectionEnabled && hasImage() && m_isCollectRoi;
    const bool allowHover  = roiSelectable && !m_roiDragging && m_roiAdjustMode == RoiAdjustMode::None;
    if(allowHover)
    {
        const QRectF sceneRoiRect = currentSceneRoiRect();
        const RoiAdjustMode hitMode = hitTestRoiHandle(scenePoint, sceneRoiRect);
        if(hitMode != m_roiHoverMode)
        {
            m_roiHoverMode = hitMode;
            updateCursorState();
        }
    }
    else if(m_roiHoverMode != RoiAdjustMode::None)
    {
        m_roiHoverMode = RoiAdjustMode::None;
        updateCursorState();
    }
}

QRectF MirrorScene::currentSceneRoiRect() const
{
    if(!hasImage() || m_roiArea.isNull())
    {
        return QRectF();
    }
    QPointF topLeft = mapImageToScene(m_roiArea.topLeft());
    QPointF bottomRight = mapImageToScene(m_roiArea.bottomRight());
    return QRectF(topLeft, bottomRight).normalized();
}

MirrorScene::RoiAdjustMode MirrorScene::hitTestRoiHandle(const QPointF &scenePos, const QRectF &scencRect) const
{
    if(scencRect.isNull()) return RoiAdjustMode::None;

    const double handleRadius = m_sourceImageSize.width()/900*10;
    const double edgeTolerance = m_sourceImageSize.width()/900*10;
    const double minInteriorPadding  = handleRadius + 1.0;
    const QPointF topLeft = scencRect.topLeft();
    const QPointF topRight = scencRect.topRight();
    const QPointF bottomLeft = scencRect.bottomLeft();
    const QPointF bottomRight = scencRect.bottomRight();

    const auto within = [&](const QPointF &corner) {
        return (std::abs(scenePos.x() - corner.x()) <= handleRadius) &&
               (std::abs(scenePos.y() - corner.y()) <= handleRadius);
    };

    if(within(topLeft)) return RoiAdjustMode::TopLeft;
    if(within(topRight)) return RoiAdjustMode::TopRight;
    if(within(bottomLeft)) return RoiAdjustMode::BottomLeft;
    if(within(bottomRight)) return RoiAdjustMode::BottomRight;

    const bool withVertital = scenePos.y() >= topLeft.y() + minInteriorPadding && scenePos.y() <= bottomLeft.y() - minInteriorPadding;
    if(withVertital){
        if(std::abs(scenePos.x() - topLeft.x()) <= edgeTolerance){
            return RoiAdjustMode::Left;
        }
        if(std::abs(scenePos.x() - topRight.x()) <= edgeTolerance){
            return RoiAdjustMode::Right;
        }
    }
    const bool withHorizontal = scenePos.x() >= topLeft.x() + minInteriorPadding && scenePos.x() <= topRight.x() - minInteriorPadding;
    if(withHorizontal){
        if(std::abs(scenePos.y() - topLeft.y()) <= edgeTolerance){
            return RoiAdjustMode::Top;
        }
        if(std::abs(scenePos.y() - bottomLeft.y()) <= edgeTolerance){
            return RoiAdjustMode::Bottom;
        }
    }

    QRectF interiorRect = scencRect.adjusted(edgeTolerance, edgeTolerance, -edgeTolerance, -edgeTolerance);
    if(!interiorRect.isNull() && interiorRect.contains(scenePos)) return RoiAdjustMode::Move;
    return RoiAdjustMode::None;

}

void MirrorScene::updateCursorState() const
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

void MirrorScene::updateDragMode() const
{
    if(!m_view) return;
    const bool enableDrag = hasImage() && !m_roiSelectionEnabled && m_roiAdjustMode == RoiAdjustMode::None;
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setHorizontalScrollBarPolicy(enableDrag ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(enableDrag ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    updateCursorState();
}

void MirrorScene::updateRoiAdjusting(const QPointF &scenePos)
{
    if(m_roiAdjustMode == RoiAdjustMode::None) return;
    QRectF rect = m_roiAdjustInitialSceneRect;
    const QPointF delta = scenePos - m_roiAdjustStartScenePos;
    switch(m_roiAdjustMode)
    {
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
    if(rect.width() < 2.0 || rect.height() < 2.0)  return;

    applySceneRoiRect(rect);
    m_roiAdjustInitialSceneRect = currentSceneRoiRect();
    m_roiAdjustStartScenePos = scenePos;
}

void MirrorScene::applySceneRoiRect(const QRectF &sceneRect)
{
    if(!hasImage() || sceneRect.isNull()) return;
    QRectF clampedRect = clampToImage(sceneRect);
    QPointF topLeftImg = mapSceneToImage(clampedRect.topLeft());
    QPointF bottomRightImg = mapSceneToImage(clampedRect.bottomRight());
    m_roiArea = QRectF(topLeftImg, bottomRightImg).normalized();
    setRoiArea(m_roiArea);
}

void MirrorScene::setRoiArea(const QRectF &roi)
{
    QRectF clampedRoi = clampToImage(roi);
    m_roiArea = clampedRoi;
    m_isCollectRoi = !m_roiArea.isNull() && m_roiArea.width() > 0.0 && m_roiArea.height() > 0.0;
    if(hasImage() && m_isCollectRoi)
    {
        QRectF sceneRoiRect(mapImageToScene(m_roiArea.topLeft()), mapImageToScene(m_roiArea.bottomRight()));
        updateRoiItem(sceneRoiRect, true);
    }
    else
    {
        updateRoiItem(QRectF(), false);
    }
}

void MirrorScene::setRoiSelectionEnabled(bool enable)
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

QRectF MirrorScene::getRoiArea() const
{
    if (!m_isCollectRoi) {
        return QRectF();
    }
    return m_roiArea;
}

void MirrorScene::clearRoi()
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

QRectF MirrorScene::clampToImage(const QRectF &imageRect) const
{
    QRectF normalized = imageRect.normalized(); // 规范化矩形方向
    if (!m_sourceImageSize.isValid() || normalized.isNull()) {
        return normalized; // 若图像尺寸无效则直接返回
    }

    QRectF bounds(QPointF(0.0, 0.0), m_sourceImageSize); // 以原图大小构造边界
    QRectF clamped = normalized.intersected(bounds); // 与边界求交确保 ROI 在图像内
    return clamped; // 返回限制后的矩形
}

void MirrorScene::onImageCleared()
{
    ImageSceneBase::onImageCleared();
    m_roiSelectionEnabled = false;
    clearRoi();
    updateDragMode();
}

void MirrorScene::onImageUpdated()
{
    ImageSceneBase::onImageUpdated();
    if (m_isCollectRoi && hasImage() && !m_roiArea.isNull()) {
        QRectF sceneRect(mapImageToScene(m_roiArea.topLeft()),
                         mapImageToScene(m_roiArea.bottomRight()));
        updateRoiItem(sceneRect.normalized(), true);
    }
    updateDragMode();
}

bool MirrorScene::event(QEvent *event)
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

void MirrorScene::onViewAttached(QGraphicsView *newView)
{
    ImageSceneBase::onViewAttached(newView);
    updateDragMode();
}
