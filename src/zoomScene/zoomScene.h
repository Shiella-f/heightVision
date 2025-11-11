#pragma once
#include <QGraphicsScene>
#include <QPixmap>
#include <QSize>
#include <QPointer>
#include <functional>


class QPointF;
class QGraphicsView;
class QGraphicsPixmapItem;
class QGraphicsTextItem;
class QAction;
class QGraphicsSceneWheelEvent;
class QGraphicsSceneContextMenuEvent;

using PointCallback = std::function<void(const QVector<QPointF>&)>;

class ZoomScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ZoomScene(QObject *parent = nullptr);

    void attachView(QGraphicsView *view);
    void setOriginalPixmap(const QPixmap &pixmap);
    void showPlaceholder(const QString &text);
    void clearImage();
    void resetImage();
    QSize availableSize() const;
    bool hasImage() const;
    void waitForPoints(int p_PointCount, PointCallback callback);
    void cancelPointCollect();
    void setSourceImageSize(const QSize &size);
    

protected:
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    QVector<QPointF> getprePoint() const{ return m_prePoints; };
    QVector<QPointF> getresultPoint() const{ return m_resultPoints; };
private:
    void ensurePlaceholderItem();
    void updateDragMode() const;
    void applyScale(double factor);
    QPointF mapSceneToImage(const QPointF &scenePoint) const;

    QPointer<QGraphicsView> m_view;
    QGraphicsPixmapItem *m_pixmapItem;
    QGraphicsTextItem *m_placeholderItem;
    QAction *m_resetAction;
    double m_scale;
    double m_minScale;
    double m_maxScale;
    bool m_hasImage;

    int m_pointCount = 0;
    QVector<QPointF> m_prePoints;
    QVector<QPointF> m_resultPoints;
    bool m_isCollectPoint = false;
    PointCallback m_pointCallback;
    double m_pointMinDistance = 5.0;
    QSizeF m_sourceImageSize;
};