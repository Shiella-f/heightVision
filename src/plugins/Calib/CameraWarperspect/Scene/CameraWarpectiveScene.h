#pragma once
#include "Scene/ImageSceneBase.h"
#include <QGraphicsSceneMouseEvent>
#include <QMessageBox>
#include <array>

class WarpectiveScene : public ImageSceneBase {
    Q_OBJECT
public:

    explicit WarpectiveScene(QObject *parent = nullptr);
    void setSelectingPoint(bool isSelecting){isSelectingPoint = isSelecting;}
    int selectedPointOrder(const QPointF point) const;

signals:
    void currentPointChanged(const QPointF &m_currentClickPos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    bool event(QEvent *event) override;     

    void onImageCleared() override;                                           // 图像清空
    void onImageUpdated() override;                                           // 图像更新时刷新拖拽/光标
    void onViewAttached(QGraphicsView *newView) override;                     // 视图绑定后同步状态

private:
    QPointF m_currentClickPos;
    bool isSelectingPoint = false; // 是否正在选择点
    QVector<QPointF> m_selectedPoints; // 存储四个选定的点

};
