#include "zoomScene/ImageSceneBase.h"

#include <QAction>
#include <QColor>
#include <QFrame>
#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QMenu>
#include <QPainter>
#include <QTransform>
#include <QtMath>
#include <algorithm>

namespace {
QRectF centeredRect(const QSizeF &size)
{
    const QPointF topLeft(-size.width() / 2.0, -size.height() / 2.0);
    return QRectF(topLeft, size);
}
}

ImageSceneBase::ImageSceneBase(QObject *parent)
    : QGraphicsScene(parent)
{
    m_resetAction = new QAction(tr("恢复原图"), this);
    connect(m_resetAction, &QAction::triggered, this, &ImageSceneBase::resetImage);
}

void ImageSceneBase::attachView(QGraphicsView *view)
{
    if (m_view == view) {
        return;
    }

    if (m_view) {
        onBeforeDetachView(m_view);
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
        m_view->setMouseTracking(true);
        if (auto *viewport = m_view->viewport()) {
            viewport->setMouseTracking(true);
        }

        QPointer<QGraphicsView> trackedView = m_view;
        connect(m_view, &QObject::destroyed, this, [this, trackedView]() {
            if (trackedView) {
                onBeforeDetachView(trackedView);
            }
            m_view = nullptr;
        });

        onViewAttached(m_view);
        updateCursorState();
    }

    resetImage();
}

void ImageSceneBase::setOriginalPixmap(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        clearImage();
        return;
    }

    if (!m_pixmapItem) {
        m_pixmapItem = addPixmap(pixmap);
        m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);
        m_pixmapItem->setAcceptedMouseButtons(Qt::NoButton);
        m_pixmapItem->setAcceptHoverEvents(false);
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
    resetImage();
    onImageUpdated();
    updateCursorState();
}

void ImageSceneBase::showPlaceholder(const QString &text)
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

    onImageCleared();
    updateCursorState();
}

void ImageSceneBase::clearImage()
{
    m_hasImage = false;
    m_scale = 1.0;
    m_minScale = 0.1;
    if (m_pixmapItem) {
        m_pixmapItem->setVisible(false);
        m_pixmapItem->setPixmap(QPixmap());
    }
    if (m_view) {
        m_view->setTransform(QTransform());
        m_view->centerOn(0.0, 0.0);
    }
    m_sourceImageSize = QSizeF();
    onImageCleared();
    updateCursorState();
}

void ImageSceneBase::resetImage()
{
    if (!m_view || !m_pixmapItem || !m_hasImage) {
        return;
    }

    m_view->setTransform(QTransform());
    m_scale = 1.0;
    fitImageToView();
}

QSize ImageSceneBase::availableSize() const
{
    if (m_view) {
        const QSize size = m_view->viewport()->size();
        if (size.width() > 0 && size.height() > 0) {
            return size;
        }
    }
    return QSize(320, 240);
}

bool ImageSceneBase::hasImage() const
{
    return m_hasImage && m_pixmapItem && !m_pixmapItem->pixmap().isNull();
}

void ImageSceneBase::setSourceImageSize(const QSize &size)
{
    m_sourceImageSize = size;
}

void ImageSceneBase::wheelEvent(QGraphicsSceneWheelEvent *event)
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

void ImageSceneBase::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
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

void ImageSceneBase::applyScale(double factor)
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

QPointF ImageSceneBase::mapSceneToImage(const QPointF &scenePoint) const
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

QPointF ImageSceneBase::mapImageToScene(const QPointF &imagePoint) const
{
    if (!m_pixmapItem) {
        return imagePoint;
    }

    const QSizeF pixSize = m_pixmapItem->pixmap().size();
    if (pixSize.isEmpty() || !m_sourceImageSize.isValid()) {
        return m_pixmapItem->mapToScene(imagePoint);
    }

    const double scaleX = pixSize.width() / m_sourceImageSize.width();
    const double scaleY = pixSize.height() / m_sourceImageSize.height();
    QPointF local(imagePoint.x() * scaleX, imagePoint.y() * scaleY);
    return m_pixmapItem->mapToScene(local);
}

void ImageSceneBase::fitImageToView()
{
    if (!m_view || !m_pixmapItem || !hasImage()) {
        return;
    }

    const QSize viewportSize = m_view->viewport()->size();
    const QRectF pixRect = m_pixmapItem->boundingRect();
    if (viewportSize.width() <= 0 || viewportSize.height() <= 0 || pixRect.isNull()) {
        m_view->centerOn(m_pixmapItem);
        return;
    }

    const double scaleX = viewportSize.width() / pixRect.width();
    const double scaleY = viewportSize.height() / pixRect.height();
    double factor = std::min(scaleX, scaleY);
    if (factor <= 0.0) {
        return;
    }

    if (factor < m_minScale) {
        m_minScale = factor;
    }

    m_view->scale(factor, factor);
    m_scale = factor;
    m_view->centerOn(m_pixmapItem);
}

void ImageSceneBase::updateCursorState() const
{
    if (m_view) {
        m_view->unsetCursor();
    }
}

void ImageSceneBase::onBeforeDetachView(QGraphicsView *oldView)
{
    Q_UNUSED(oldView)
}

void ImageSceneBase::onViewAttached(QGraphicsView *newView)
{
    Q_UNUSED(newView)
}

void ImageSceneBase::onImageCleared()
{
}

void ImageSceneBase::onImageUpdated()
{
}

void ImageSceneBase::ensurePlaceholderItem()
{
    if (!m_placeholderItem) {
        m_placeholderItem = addText(QString());
        m_placeholderItem->setZValue(1.0);
        m_placeholderItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_placeholderItem->setFlag(QGraphicsItem::ItemIsMovable, false);
        m_placeholderItem->setAcceptedMouseButtons(Qt::NoButton);
        m_placeholderItem->setAcceptHoverEvents(false);
    }
}
