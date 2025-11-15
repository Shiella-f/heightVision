#pragma once

#include <QGraphicsScene>
#include <QPointer>
#include <QPixmap>
#include <QSizeF>

class QAction;
class QGraphicsSceneWheelEvent;
class QGraphicsSceneContextMenuEvent;
class QGraphicsPixmapItem;
class QGraphicsTextItem;
class QGraphicsView;
class QPointF;
class QSize;
class QString;

// ImageSceneBase 提供绑定视图、展示图像及基础交互能力，派生类只需专注于特定业务逻辑
class ImageSceneBase : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ImageSceneBase(QObject *parent = nullptr);

    void attachView(QGraphicsView *view);              // 绑定视图并完成通用配置
    void setOriginalPixmap(const QPixmap &pixmap);     // 设置原始图像
    void showPlaceholder(const QString &text);         // 显示占位符
    virtual void clearImage();                         // 清空当前图像
    void resetImage();                                 // 恢复初始视图
    QSize availableSize() const;                       // 视口可用尺寸
    bool hasImage() const;                             // 是否存在有效图像
    void setSourceImageSize(const QSize &size);        // 保存原图尺寸

protected:
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;             // 通用滚轮缩放
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override; // 恢复原图菜单

    void applyScale(double factor);                   // 执行缩放
    QPointF mapSceneToImage(const QPointF &scenePoint) const; // 场景到图像坐标
    QPointF mapImageToScene(const QPointF &imagePoint) const; // 图像到场景坐标
    void fitImageToView();                            // 视图自适应
    virtual void updateCursorState() const;           // 供子类定义鼠标样式

    virtual void onBeforeDetachView(QGraphicsView *oldView);   // 视图解绑前回调
    virtual void onViewAttached(QGraphicsView *newView);       // 视图绑定后回调
    virtual void onImageCleared();                              // 图像清空
    virtual void onImageUpdated();                              // 图像更新

protected:
    QPointer<QGraphicsView> m_view;                    // 当前绑定的视图
    QGraphicsPixmapItem *m_pixmapItem = nullptr;       // 显示图像的图元
    QGraphicsTextItem *m_placeholderItem = nullptr;    // 占位文本
    QAction *m_resetAction = nullptr;                  // 重置动作
    double m_scale = 1.0;                              // 当前缩放
    double m_minScale = 0.1;                           // 最小缩放
    double m_maxScale = 5.0;                           // 最大缩放
    bool m_hasImage = false;                           // 是否有图像
    QSizeF m_sourceImageSize;                          // 原始尺寸

private:
    void ensurePlaceholderItem();                      // 确保占位项存在
};
