#pragma once
#include "Scene/ImageSceneBase.h"
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QColor>
#include <QPainterPathStroker>
#include <opencv2/opencv.hpp>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include "template/template_global.h"

class QGraphicsTextItem; // 用于显示图片尺寸的文本项前向声明（修改）

// MatchScene: 继承自 ImageSceneBase 的可交互 ROI 选择场景
class TEMPLATE_EXPORT MatchScene : public ImageSceneBase {
    Q_OBJECT
public:
    enum CompositionMode { United = 0, Intersected = 1, Subtracted = 2 };
    enum ShapeMode { Rect = 0, Ellipse = 1, Free = 2 };

    explicit MatchScene(QObject *parent = nullptr);

    void setSelectionTemplateEnabled(bool enable); // 开关模板区域绘制功能
    void setRoiAreaSelectionEnabled(bool enable); // 开关匹配区域选择功能

    void setCompositionMode(CompositionMode m);
    void setShapeMode(ShapeMode s);
    void setPenWidth(int w);
    void clear();

    QPointF imageToScenePoint(const QPointF &pt) const; // 将图像像素坐标映射到场景坐标
    QPointF sceneToImagePoint(const QPointF &pt) const; // 将场景坐标映射回图像像素坐标


    cv::Mat extractTemplate() const; // 使用合成路径生成掩码并从源图像中裁剪模板
    cv::Mat extractMatchAreaMask() const; // 提取矩形区域
    QRect getMatchAreaBoundingRect() const; // 返回extractMatchAreaMask包围盒

    QRect getTemplateBoundingRect() const; // 返回extractTemplate包围盒

    void DrawTotalPath(QVector<QPainterPath> paths); // 在场景中绘制合成后的 ROI 路径

    QVector<QPainterPath> getDrawPaths() const { return drawPaths; } // 获取当前绘制的路径集合 

    

signals:
    void totalPathPathChanged(const QPainterPath &path);
    void roiChanged();
    void currentPointChanged(const QPointF &m_currentClickPos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void onImageUpdated() override;
    void onImageCleared() override;

private:
    QPointF m_currentClickPos;
    
    QPainterPath m_currentPath;
    QPainterPath m_totalPath;
    CompositionMode m_compMode = United;
    ShapeMode m_shapeMode = Rect;
    int m_penWidth = 3;
    QPointF m_pressPos;
    QRectF m_tempRect; // 临时矩形，用于矩形/椭圆预览
    bool m_drawing = false; // 是否正在绘制
    bool m_selectionTemplateEnabled = true; // 是否允许选择/绘制

    QGraphicsPathItem* m_previewPathItem = nullptr; // 自由绘制预览图元
    QGraphicsRectItem* m_previewRectItem = nullptr; // 矩形预览图元
    QGraphicsEllipseItem* m_previewEllipseItem = nullptr; // 椭圆预览图元

    QGraphicsPathItem* m_totalPathItem = nullptr; // 用于在场景中显示合成后的 ROI

    mutable cv::Rect m_lastExtractBbox; // 保存上次 extractTemplate 计算的包围盒

    bool m_RoiAreaSelectionEnabled = false; // 是否启用矩形选择模式
    QGraphicsRectItem* m_matchAreaItem = nullptr; // 场景中的矩形选择项（用于显示与交互）
    QGraphicsRectItem* m_rotateHandleItem = nullptr; // 旋转把手（现在使用小矩形而非圆形）
    QGraphicsRectItem* m_cornerHandleItems[4] = {nullptr, nullptr, nullptr, nullptr}; // 四个角点把手（小红色矩形）
    QPointF m_matchPressPos; // 记录鼠标按下时的场景坐标（用于拖动/缩放/旋转计算）
    bool m_matchDragging = false; // 是否处于拖动（移动或新建矩形）状态
    bool m_matchResizing = false; // 是否处于角点缩放状态
    bool m_matchRotating = false; // 是否处于旋转状态
    enum MatchResizeCorner { MR_None=0, MR_TopLeft=1, MR_TopRight=2, MR_BottomLeft=3, MR_BottomRight=4, MR_Move=5 }; // 缩放/移动的枚举角点
    MatchResizeCorner m_matchResizeCorner = MR_None; // 当前正在操作的角点类型
    double m_matchRotation = 0.0; // 矩形的当前旋转角度（以度为单位）
    QRectF m_matchSceneRect; // 矩形在场景坐标系下的边界（不含旋转变换）
    mutable cv::Rect m_lastMatchBbox; // 保存上次 extractMatchArea 计算并用于裁剪的像素坐标包围盒
    QGraphicsTextItem* m_imageSizeItem = nullptr; // 场景中显示当前图片大小的文本项（修改）
    QVector<QPainterPath> drawPaths; // 用于存储绘制的多个路径
    

};



// ---------------- 交互式路径的辅助类定义 ----------------

// RotateHandle: 用于旋转 InteractivePathItem 的把手图元
class RotateHandle : public QGraphicsRectItem {
public:
    // 构造函数：初始化把手的大小、颜色和光标
    RotateHandle(QGraphicsItem* parent) : QGraphicsRectItem(-5, -5, 10, 10, parent) {
        setBrush(Qt::red); // 设置填充颜色为红色
        setPen(QPen(Qt::white)); // 设置边框颜色为白色
        setFlag(ItemIsMovable, false); // 把手本身不可移动（相对于父项）
        setFlag(ItemIsSelectable, false); // 把手不需要被选中
        //setCursor(Qt::PointingHandCursor); // 鼠标悬停时显示手型光标
    }
protected:
    // 鼠标按下事件：记录按下的位置和父项当前的旋转角度
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        m_pressPos = event->scenePos(); // 记录鼠标按下时的场景坐标
        m_startRotation = parentItem()->rotation(); // 记录父项当前的旋转角度
        event->accept(); // 接受事件，防止传递给下层
    }
    // 鼠标移动事件：计算旋转角度并应用于父项
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        QGraphicsItem* pItem = parentItem(); // 获取父项指针
        if(!pItem) return; // 如果没有父项则直接返回
        
        // 获取旋转中心在场景坐标系中的位置
        QPointF center = pItem->mapToScene(pItem->transformOriginPoint());
        QPointF curPos = event->scenePos(); // 获取当前鼠标位置

        // 计算按下点相对于中心的角度
        double a1 = std::atan2(m_pressPos.y() - center.y(), m_pressPos.x() - center.x());
        // 计算当前点相对于中心的角度
        double a2 = std::atan2(curPos.y() - center.y(), curPos.x() - center.x());
        // 计算角度差（增量），转换为度数
        double delta = (a2 - a1) * 180.0 / 3.14159265358979323846;

        // 设置父项新的旋转角度（初始角度 + 增量）
        pItem->setRotation(m_startRotation + delta);
        event->accept(); // 接受事件
    }
private:
    QPointF m_pressPos; // 鼠标按下时的场景坐标
    qreal m_startRotation = 0.0; // 交互开始时的旋转角度
};

// InteractivePathItem: 可交互（选中、移动、旋转）的路径图元
class InteractivePathItem : public QGraphicsPathItem {
public:
    // 构造函数：初始化路径、外观、标志位和旋转把手
    InteractivePathItem(const QPainterPath& path) : QGraphicsPathItem(path) {
        // 设置标志：可移动、可选中、几何变化时发送通知
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setPen(QPen(Qt::black, 2)); // 设置路径轮廓为黑色，宽度2
        setBrush(QColor(0, 255, 0, 50)); // 设置填充为半透明绿色
        
        QRectF br = boundingRect(); // 获取路径的边界矩形
        setTransformOriginPoint(br.center()); // 设置变换（旋转）中心为矩形中心

        m_handle = new RotateHandle(this); // 创建旋转把手，作为子项
        // 将把手放置在中心上方 20 像素处
        m_handle->setPos(br.center().x(), br.top() - 20);
        m_handle->setVisible(false); // 初始状态隐藏把手（选中时才显示）
    }
    
    // 定义图元类型 ID，用于类型转换和识别
    enum { Type = UserType + 100 };
    int type() const override { return Type; }

protected:
    // 图元状态改变事件：处理选中状态变化
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemSelectedHasChanged) { // 如果选中状态发生改变
            if (m_handle) m_handle->setVisible(value.toBool()); // 根据是否选中显示或隐藏旋转把手
        }
        return QGraphicsPathItem::itemChange(change, value); // 调用基类实现
    }

private:
    RotateHandle* m_handle = nullptr; // 旋转把手指针
};


