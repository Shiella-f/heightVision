#include "Scene/ImageDisplayScene.h"

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
