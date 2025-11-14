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
class QGraphicsRectItem;
class QAction;
class QGraphicsSceneWheelEvent;
class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneMouseEvent;

// ZoomScene 提供图像缩放、拖拽及手动拾取点的图形场景
class ZoomScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ZoomScene(QObject *parent = nullptr); // 初始化交互状态

    void attachView(QGraphicsView *view);            // 绑定外部视图，统一缩放拖拽
    void setOriginalPixmap(const QPixmap &pixmap);   // 设置原始图像
    void showPlaceholder(const QString &text);       // 显示占位提示文本
    void clearImage();                               // 清理当前图像内容
    void resetImage();                               // 恢复缩放与位置
    QSize availableSize() const;                     // 获取可用显示区域尺寸
    bool hasImage() const;                           // 是否已加载图像
    void setSourceImageSize(const QSize &size);      // 记录源图像尺寸

    void setRoiArea(const QRectF &roi);               // 设置用户选取的ROI区域
    bool getRoiArea(QRectF &roi) const;               // 获取用户选取的ROI区域
    void enableRoiSelection(bool enable);             // 开启/关闭 ROI 拾取模式
    void setRoiSelectionEnabled(bool enabled);         // 设置 ROI 拾取模式
    bool isRoiSelectionEnabled() const;               // 当前是否允许拾取 ROI
    void clearRoi();                                  // 清除已选 ROI
    

protected:
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;                // 支持滚轮缩放
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;    // 右键菜单
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;           // 处理点拾取与拖拽
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;            // ROI 框选时动态更新
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;         // 完成 ROI 选取

private:
    void ensurePlaceholderItem();                     // 确保占位文本项存在
    void updateDragMode() const;                      // 根据状态更新拖拽模式
    void applyScale(double factor);                   // 应用缩放因子
    QPointF mapSceneToImage(const QPointF &scenePoint) const; // 场景坐标转图像坐标
    QPointF mapImageToScene(const QPointF &imagePoint) const; // 图像坐标转场景坐标
    void ensureRoiItem();                              // 确保 ROI 可视化矩形存在
    void updateRoiItem(const QRectF &sceneRect, bool visible); // 刷新 ROI 显示
    QRectF clampToImage(const QRectF &imageRect) const;        // 将 ROI 限制在图像范围内

    QPointer<QGraphicsView> m_view;                   // 关联的视图对象
    QGraphicsPixmapItem *m_pixmapItem;                // 展示图像的图元
    QGraphicsTextItem *m_placeholderItem;             // 占位文字图元
    QAction *m_resetAction;                           // 重置动作
    double m_scale;                                   // 当前缩放比例
    double m_minScale;                                // 最小缩放限制
    double m_maxScale;                                // 最大缩放限制
    bool m_hasImage;                                  // 是否已加载图像

    QSizeF m_sourceImageSize;                         // 原图尺寸用于坐标换算

    bool m_roiSelectionEnabled = false;               // 是否允许拾取 ROI
    bool m_isCollectRoi = false;                      // 是否已选中 ROI
    QPointF m_roiStartScene;                          // ROI 起点（场景坐标）
    bool m_roiDragging = false;                       // 是否正处于拖拽
    QRectF m_roiArea;                                 // 用户选取的ROI区域（图像坐标）
    QGraphicsRectItem *m_roiRectItem = nullptr;       // 可视化 ROI 的矩形
};