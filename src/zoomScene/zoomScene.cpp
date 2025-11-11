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
    m_hasImage = false;
    m_scale = 1.0;
    if (m_pixmapItem) {
        m_pixmapItem->setVisible(false);
        m_pixmapItem->setPixmap(QPixmap());
    }
    if (m_view) {
        m_view->setTransform(QTransform());
        m_view->centerOn(0.0, 0.0);
    }
    m_sourceImageSize = QSizeF();
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

    const bool enableDrag = hasImage();
    // enableDrag 为真表示场景中存在可显示的图片，此时允许通过鼠标拖拽移动视图。
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
    if (!m_isCollectPoint) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::RightButton) {
        // 右键视为用户主动取消点选，清理状态并通知回调。
        const PointCallback callback = m_pointCallback;
        cancelPointCollect();
        if (callback) {
            callback(QVector<QPointF>());
        }
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    if (!hasImage()) {
        event->ignore();
        return;
    }

    const QPointF scenePos = event->scenePos();
    if (m_pixmapItem) {
        const QPointF localPos = m_pixmapItem->mapFromScene(scenePos);
        if (!m_pixmapItem->contains(localPos)) {
            // 如果点击落在图像之外，直接忽略，避免记录无效坐标。
            event->accept();
            return;
        }
    }

    const QPointF imagePos = mapSceneToImage(scenePos);
    if (m_sourceImageSize.isValid()) {
        if (imagePos.x() < 0.0 || imagePos.y() < 0.0 || imagePos.x() >= m_sourceImageSize.width() || imagePos.y() >= m_sourceImageSize.height()) {
            event->accept();
            return;
        }
    }

    // 检查新点与已选点的间距，避免用户误触同一位置。
    const bool tooClose = std::any_of(m_prePoints.cbegin(), m_prePoints.cend(),
                                      [this, &imagePos](const QPointF &p) {
                                          return QLineF(p, imagePos).length() < m_pointMinDistance;
                                      });
    if (tooClose) {
        event->accept();
        return;
    }

    m_prePoints.append(imagePos);

    // 检查是否已收集到足够的点，若是则结束点选流程并调用回调。
    if (m_prePoints.size() >= m_pointCount) {
        m_resultPoints = m_prePoints;
        const PointCallback callback = m_pointCallback;
        m_pointCallback = nullptr;
        m_isCollectPoint = false;
        updateDragMode();
        if (callback) {
            callback(m_resultPoints);
        }
        event->accept();
        return;
    }

    event->accept();
}

void ZoomScene::waitForPoints(int p_PointCount, PointCallback callback)
{
    if (!hasImage()) {
        if (callback) {
            callback(QVector<QPointF>());
        }
        return;
    }

    if (p_PointCount <= 0) {
        if (callback) {
            callback(QVector<QPointF>());
        }
        return;
    }

    // 启动新的点收集流程前，先清理旧状态，确保不会残留历史数据。
    cancelPointCollect();

    m_isCollectPoint = true;
    m_pointCount = p_PointCount;
    m_pointCallback = std::move(callback);
    m_prePoints.clear();
    m_resultPoints.clear();

    if (m_view) {
        // 点选阶段禁用拖拽，改用十字光标提示当前处于取点模式。
        m_view->setDragMode(QGraphicsView::NoDrag);
        m_view->setCursor(Qt::CrossCursor);
    }

    update();
}

void ZoomScene::cancelPointCollect()
{
    m_isCollectPoint = false;
    m_pointCount = 0;
    m_prePoints.clear();
    m_resultPoints.clear();
    m_pointCallback = nullptr;
    if (m_view) {
        updateDragMode();
    }
    update();
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

