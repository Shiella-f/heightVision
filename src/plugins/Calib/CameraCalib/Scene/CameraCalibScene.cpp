#include "CameraCalibScene.h"


CameraCalibScene::CameraCalibScene(QObject *parent)
    : ImageSceneBase(parent)
{
    
}

void CameraCalibScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) 
    {
        const QPointF imgPt = mapSceneToImage(event->scenePos());
        m_currentClickPos = imgPt;
        emit currentPointChanged(m_currentClickPos);
    }
    QGraphicsScene::mousePressEvent(event);
}

void CameraCalibScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseMoveEvent(event);
}

void CameraCalibScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
}