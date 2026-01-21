#include "ImageDisplayScene.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QtMath>
#include <algorithm>

namespace {
QRectF centeredRect(const QSizeF &size)
{
    const QPointF topLeft(-size.width() / 2.0, -size.height() / 2.0);
    return QRectF(topLeft, size);
}
}

ImageDisplayScene::ImageDisplayScene(QObject *parent)
    : QGraphicsScene(parent)
{
    
}

void ImageDisplayScene::setOriginalPixmap(const QPixmap &pixmap)
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
        m_pixmapItem->setPos(0, 0);
        m_pixmapItem->setPixmap(pixmap);
        m_pixmapItem->setVisible(true);
    }

    if (m_placeholderItem) {
        m_placeholderItem->setVisible(false);
    }

    m_hasImage = true;
    setSceneRect(m_pixmapItem->boundingRect());
    m_sourceImageSize = pixmap.size();
    onImageUpdated();
}

QImage ImageDisplayScene::getSourceImageMat() const
{
    if (m_pixmapItem && !m_pixmapItem->pixmap().isNull()) {
        return m_pixmapItem->pixmap().toImage();
    }
    return QImage();
}

void ImageDisplayScene::showPlaceholder(const QString &text)
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

    onImageCleared();
}

void ImageDisplayScene::clearImage()
{
    m_hasImage = false;
    if (m_pixmapItem) {
        m_pixmapItem->setVisible(false);
        m_pixmapItem->setPixmap(QPixmap());
    }
    m_sourceImageSize = QSizeF();
    onImageCleared();
}

bool ImageDisplayScene::hasImage() const
{
    return m_hasImage && m_pixmapItem && !m_pixmapItem->pixmap().isNull();
}

void ImageDisplayScene::setSourceImageSize(const QSize &size)
{
    m_sourceImageSize = size;
}

QPointF ImageDisplayScene::mapSceneToImage(const QPointF &scenePoint) const
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

QPointF ImageDisplayScene::mapImageToScene(const QPointF &imagePoint) const
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

void ImageDisplayScene::onImageCleared()
{
    
}

void ImageDisplayScene::onImageUpdated()
{
    
}

void ImageDisplayScene::ensurePlaceholderItem()
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

QGraphicsRectItem* ImageDisplayScene::addOverlayRect(const QRect &imageRect, const QPen &pen)
{
    if (imageRect.isEmpty()) return nullptr;
    QPointF tl = mapImageToScene(QPointF(imageRect.x(), imageRect.y()));
    QPointF br = mapImageToScene(QPointF(imageRect.x() + imageRect.width(), imageRect.y() + imageRect.height()));
    QRectF sceneRect(tl, br);
    // 创建矩形覆盖物并设置优先级/不可交互，以便作为纯显示元素
    QGraphicsRectItem* item = addRect(sceneRect, pen);
    item->setZValue(5.0);
    // 覆盖物不应被选中或移动
    item->setFlag(QGraphicsItem::ItemIsSelectable, false);
    item->setFlag(QGraphicsItem::ItemIsMovable, false);
    item->setData(0, QVariant(true));
    return item;
}

QGraphicsEllipseItem* ImageDisplayScene::addOverlayPoint(const QPointF &imagePoint, const QPen &pen, double radius)
{
    QPointF center = mapImageToScene(imagePoint);
    QRectF r(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
    // 创建椭圆覆盖物（用于显示特征点）并保证其可见性
    QGraphicsEllipseItem* item = addEllipse(r, pen, QBrush(pen.color()));
    item->setZValue(5.0);
    item->setFlag(QGraphicsItem::ItemIsSelectable, false);
    item->setFlag(QGraphicsItem::ItemIsMovable, false);
    item->setData(0, QVariant(true));
    return item;
}

QGraphicsPolygonItem* ImageDisplayScene::addOverlayPolygon(const QPolygonF &imagePoly, const QPen &pen)
{
    if (imagePoly.isEmpty()) return nullptr;
    QPolygonF scenePoly;
    scenePoly.reserve(imagePoly.size());
    for (const QPointF &pt : imagePoly) {
        scenePoly << mapImageToScene(pt);
    }
    QGraphicsPolygonItem* item = addPolygon(scenePoly, pen);
    if (item) {
        item->setZValue(5.0);
        item->setFlag(QGraphicsItem::ItemIsSelectable, false);
        item->setFlag(QGraphicsItem::ItemIsMovable, false);
        item->setData(0, QVariant(true));
    }
    return item;
}

void ImageDisplayScene::clearOverlays()
{
    const auto itemsList = items();
    for (QGraphicsItem* it : itemsList) {
        QVariant v = it->data(0);
        if (v.isValid() && v.toBool()) {
            removeItem(it);
            delete it;
        }
    }
}
