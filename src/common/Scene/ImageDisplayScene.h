#pragma once

#include <QGraphicsScene>
#include <QPixmap>
#include <QSizeF>
#include <QRect>
#include <QPen>
#include <QBrush>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>

class QGraphicsPixmapItem;
class QGraphicsTextItem;
class QPointF;
class QSize;
class QString;
//#include "template/template_global.h"

// ImageDisplayScene 仅负责在场景中展示图像和占位符
class ImageDisplayScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ImageDisplayScene(QObject *parent = nullptr);

    virtual void setOriginalPixmap(const QPixmap &pixmap);
    virtual void showPlaceholder(const QString &text);
    virtual QImage getSourceImageMat() const;
    virtual void clearImage();
    bool hasImage() const;
    void setSourceImageSize(const QSize &size);
    QGraphicsRectItem* addOverlayRect(const QRect &imageRect, const QPen &pen = QPen(Qt::red));
    QGraphicsEllipseItem* addOverlayPoint(const QPointF &imagePoint, const QPen &pen = QPen(Qt::green), double radius = 2.0);
    QGraphicsPolygonItem* addOverlayPolygon(const QPolygonF &imagePoly, const QPen &pen = QPen(Qt::red));
    void clearOverlays();

protected:
    QPointF mapSceneToImage(const QPointF &scenePoint) const;
    QPointF mapImageToScene(const QPointF &imagePoint) const;

    virtual void onImageCleared();//预留接口
    virtual void onImageUpdated();//预留接口

protected:
    QGraphicsPixmapItem *m_pixmapItem = nullptr;
    QGraphicsTextItem *m_placeholderItem = nullptr;
    bool m_hasImage = false;
    QSizeF m_sourceImageSize;

private:
    void ensurePlaceholderItem();
};
