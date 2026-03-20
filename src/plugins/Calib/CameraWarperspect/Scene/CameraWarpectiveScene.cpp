#include "CameraWarpectiveScene.h"
#include <QColor>
#include <QGraphicsView>


WarpectiveScene::WarpectiveScene(QObject *parent)
    : ImageSceneBase(parent)
{
    
}

void WarpectiveScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) 
    {
        const QPointF imgPt = mapSceneToImage(event->scenePos());
        m_currentClickPos = imgPt;
        emit currentPointChanged(m_currentClickPos);
    }
    if(event->button() == Qt::LeftButton && isSelectingPoint && hasImage())
    {
        if(m_selectedPoints.size() > 4) {
            m_selectedPoints.clear();
        }
        if(m_selectedPoints.size() == 4)
        {
            QMessageBox::StandardButton result = QMessageBox::question(nullptr,                
            "确认操作",               // 标题
            "已选择4个点，是否重新选择\n", // 提示文本
            QMessageBox::Yes | QMessageBox::No, // 按钮组合
            QMessageBox::No           // 默认选中“否”
        );

        // 根据用户选择执行逻辑
        if (result == QMessageBox::Yes) {
            m_selectedPoints.clear();
        } else if (result == QMessageBox::No) {
            isSelectingPoint = false; // 退出选择点模式
            return ; // 不清空，直接返回
        }
        } // 如果已经选择了4个点，清空重新选择
        const QPointF imgPt = mapSceneToImage(event->scenePos());
        m_currentClickPos = imgPt;
        m_selectedPoints.append(imgPt);
        emit currentPointChanged(m_selectedPoints.last());
        event->accept();
        return;
    }

    QGraphicsScene::mousePressEvent(event);
}

void WarpectiveScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{

    QGraphicsScene::mouseMoveEvent(event);
}

void WarpectiveScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{

    QGraphicsScene::mouseReleaseEvent(event);
}

void WarpectiveScene::onImageCleared()
{
    ImageSceneBase::onImageCleared();
}

void WarpectiveScene::onImageUpdated()
{
    ImageSceneBase::onImageUpdated();
}

bool WarpectiveScene::event(QEvent *event)
{
    return ImageSceneBase::event(event);
}

void WarpectiveScene::onViewAttached(QGraphicsView *newView)
{
    ImageSceneBase::onViewAttached(newView);
}

int WarpectiveScene::selectedPointOrder(const QPointF point) const
{
    for(auto &it : m_selectedPoints)
    {
        if(point == it)
        {
            return m_selectedPoints.indexOf(it);
        }
    }
    return -1; // 没有选中任何点
}