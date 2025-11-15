#pragma once

#include "Scene/ImageDisplayScene.h"

#include <QPointer>
#include <QSize>

class QAction;
class QGraphicsSceneWheelEvent;
class QGraphicsSceneContextMenuEvent;
class QGraphicsView;
class QPointF;

// ImageSceneBase 在显示图像的基础上提供视图绑定、缩放等交互
class ImageSceneBase : public ImageDisplayScene {
    Q_OBJECT
public:
    explicit ImageSceneBase(QObject *parent = nullptr);

    void attachView(QGraphicsView *view);              // 绑定视图并完成通用配置
    void resetImage();                                 // 恢复初始视图
    QSize availableSize() const;                       // 视口可用尺寸

protected:
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;             // 通用滚轮缩放
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override; // 恢复原图菜单

    void applyScale(double factor);                   // 执行缩放
    void fitImageToView();                            // 视图自适应
    virtual void updateCursorState() const;           // 供子类定义鼠标样式

    virtual void onBeforeDetachView(QGraphicsView *oldView);   // 视图解绑前回调
    virtual void onViewAttached(QGraphicsView *newView);       // 视图绑定后回调
    void onImageCleared() override;                             // 图像清空
    void onImageUpdated() override;                             // 图像更新

protected:
    QPointer<QGraphicsView> m_view;                    // 当前绑定的视图
    QAction *m_resetAction = nullptr;                  // 重置动作
    double m_scale = 1.0;                              // 当前缩放
    double m_minScale = 0.1;                           // 最小缩放
    double m_maxScale = 5.0;                           // 最大缩放

private:
};
