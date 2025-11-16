#pragma once

#include <QGraphicsScene>
#include <QPixmap>
#include <QSizeF>

class QGraphicsPixmapItem;
class QGraphicsTextItem;
class QPointF;
class QSize;
class QString;

// ImageDisplayScene 仅负责在场景中展示图像和占位符
class ImageDisplayScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ImageDisplayScene(QObject *parent = nullptr);

    virtual void setOriginalPixmap(const QPixmap &pixmap);   // 加载原图
    virtual void showPlaceholder(const QString &text);       // 显示占位信息
    virtual QImage getSourceImageMat() const;                // 获取原始图像的cv::Mat表示
    virtual void clearImage();                               // 清空图像内容
    bool hasImage() const;                                   // 当前是否有有效图像
    void setSourceImageSize(const QSize &size);              // 记录原图尺寸

protected:
    QPointF mapSceneToImage(const QPointF &scenePoint) const; // 场景坐标 -> 图像坐标
    QPointF mapImageToScene(const QPointF &imagePoint) const; // 图像坐标 -> 场景坐标

    virtual void onImageCleared();   // 图像清空
    virtual void onImageUpdated();   // 图像更新

protected:
    QGraphicsPixmapItem *m_pixmapItem = nullptr;   // 展示图像的图元
    QGraphicsTextItem *m_placeholderItem = nullptr;// 占位文本
    bool m_hasImage = false;                       // 是否存在有效图像
    QSizeF m_sourceImageSize;                      // 原始尺寸用于坐标换算

private:
    void ensurePlaceholderItem();                  // 确保占位图元存在
};
