#pragma once
#include "Scene/ImageSceneBase.h"
#include <QGraphicsSceneMouseEvent>
#include "../Function/CameraCalibPort.h"

class CameraCalibScene : public ImageSceneBase, public ICameraCalibScene {
    Q_OBJECT
public:

    explicit CameraCalibScene(QObject *parent = nullptr);
    void setOriPixmap(const QPixmap& pixmap) override;
signals:
    void currentPointChanged(const QPointF &m_currentClickPos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QPointF m_currentClickPos;
};
