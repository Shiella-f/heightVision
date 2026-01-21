#pragma once
#include "Scene/ImageSceneBase.h"
#include <QGraphicsSceneMouseEvent>

class CameraCalibScene : public ImageSceneBase {
    Q_OBJECT
public:

    explicit CameraCalibScene(QObject *parent = nullptr);
signals:
    void currentPointChanged(const QPointF &m_currentClickPos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QPointF m_currentClickPos;
};
