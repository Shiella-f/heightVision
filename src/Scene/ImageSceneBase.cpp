#include "Scene/ImageSceneBase.h"

#include <QAction>
#include <QFrame>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QMenu>
#include <QTransform>
#include <QtMath>
#include <QtGlobal>
#include <algorithm>

ImageSceneBase::ImageSceneBase(QObject *parent)
    : ImageDisplayScene(parent)
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
    ImageDisplayScene::onImageCleared();
    m_scale = 1.0;
    m_minScale = 0.1;
    if (m_view) {
        m_view->setTransform(QTransform());
        if (!m_hasImage && m_placeholderItem) {
            m_view->centerOn(m_placeholderItem);
        } else {
            m_view->centerOn(0.0, 0.0);
        }
    }
    updateCursorState();
}

void ImageSceneBase::onImageUpdated()
{
    ImageDisplayScene::onImageUpdated();
    resetImage();
    updateCursorState();
}
