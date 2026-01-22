#pragma once
#include "Scene/ImageSceneBase.h"
#include <QGraphicsSceneMouseEvent>
#include <array>

class MirrorScene : public ImageSceneBase {
    Q_OBJECT
public:

    explicit MirrorScene(QObject *parent = nullptr);
    void setRoiArea(const QRectF &roi);
    QRectF getRoiArea() const;
    void clearRoi(); 
    void setRoiSelectionEnabled(bool enabled);        // 设置 ROI 拾取模式

signals:
    void currentPointChanged(const QPointF &m_currentClickPos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void updateCursorState() const override;
    bool event(QEvent *event) override;     

    //void onViewAttached(QGraphicsView *newView) override;                     // 视图绑定后同步状态
    void onImageCleared() override;                                           // 图像清空时同步 ROI 状态
    void onImageUpdated() override;                                           // 图像更新时刷新拖拽/光标
    void onViewAttached(QGraphicsView *newView) override;                     // 视图绑定后同步状态

private:
    QPointF m_currentClickPos;

    void updateDragMode() const;                      // 根据状态更新拖拽模式
    void ensureRoiItem();                              // 确保 ROI 可视化矩形存在
    void updateRoiItem(const QRectF &sceneRect, bool visible); // 刷新 ROI 显示
    QRectF clampToImage(const QRectF &imageRect) const;        // 将 ROI 限制在图像范围内
    QRectF currentSceneRoiRect() const;                // 当前 ROI 的场景坐标矩形
    void applySceneRoiRect(const QRectF &sceneRect);   // 将场景 ROI 同步回图像坐标
    enum class RoiAdjustMode {None, Move, TopLeft, TopRight, BottomLeft, BottomRight, Left, Right, Top, Bottom};
    RoiAdjustMode hitTestRoiHandle(const QPointF &scenePos, const QRectF &sceneRect) const; // 命中测试
    void updateRoiAdjusting(const QPointF &scenePos);  // 实时调整 ROI
    void updateRoiHoverState(const QPointF &scenePos); // 根据位置刷新悬停状态


    bool m_roiSelectionEnabled = false;               // 是否允许拾取 ROI
    bool m_isCollectRoi = false;                      // 是否已选中 ROI
    QPointF m_roiStartScene;                          // ROI 起点（场景坐标）
    bool m_roiDragging = false;                       // 是否正处于拖拽
    QRectF m_roiArea;                                 // 用户选取的ROI区域（图像坐标）
    QGraphicsRectItem *m_roiRectItem = nullptr;       // 可视化 ROI 的矩形
    std::array<QGraphicsRectItem*, 4> m_roiHandleItems{ {nullptr, nullptr, nullptr, nullptr} }; // ROI 角点手柄
    RoiAdjustMode m_roiAdjustMode = RoiAdjustMode::None; // ROI 调整模式
    mutable RoiAdjustMode m_roiHoverMode = RoiAdjustMode::None; // 悬停模式
    QRectF m_roiAdjustInitialSceneRect;               // 调整开始时的场景矩形
    QPointF m_roiAdjustStartScenePos;                 // 调整起始场景坐标
};
