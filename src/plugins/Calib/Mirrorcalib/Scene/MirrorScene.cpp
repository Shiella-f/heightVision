#include "MirrorScene.h"


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
    QGraphicsScene::mousePressEvent(event);
}

void MirrorScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseMoveEvent(event);
}

void MirrorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
}