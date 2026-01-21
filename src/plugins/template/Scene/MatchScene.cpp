#include "MatchScene.h"
#include "tools/bscvTool.h"
#include <QPainter>
#include <QPainterPathStroker>
#include <algorithm>
#include <QPen>
#include <QDebug>
#include <QGraphicsView>
#include <QString>
#include <QPainterPath>

MatchScene::MatchScene(QObject *parent)
    : ImageSceneBase(parent)
{
    m_drawing = false;
    m_compMode = United;
    m_shapeMode = Rect;
    m_penWidth = 3;
    m_selectionTemplateEnabled = false;
    m_previewPathItem = nullptr;
    m_previewRectItem = nullptr;
    m_previewEllipseItem = nullptr; 
    m_totalPathItem = nullptr;

    m_matchAreaItem = nullptr;
    m_rotateHandleItem = nullptr;
    m_matchRotation = 0.0;
    // 创建用于显示当前图片尺寸的文本项（初始隐藏）——修改
    m_imageSizeItem = addText(QString());
    if (m_imageSizeItem) {
        QFont f = m_imageSizeItem->font();
        f.setPointSize(10);
        m_imageSizeItem->setFont(f);
        m_imageSizeItem->setDefaultTextColor(QColor(200,200,200));
        m_imageSizeItem->setZValue(10.0); // 置于最上层以免被覆盖（修改）
        m_imageSizeItem->setVisible(false);
    }
    resetImage();
}

// 当场景的图像被更新时，刷新显示的图片尺寸（修改）
void MatchScene::onImageUpdated()
{
    ImageSceneBase::onImageUpdated();
    if (!m_imageSizeItem) return;
    if (m_sourceImageSize.isValid()) {
        // 使用 image 尺寸显示为 "WxH" 格式，并放置在图像左上方少量偏移处（修改）
        QString txt = QStringLiteral("%1 x %2").arg(static_cast<int>(m_sourceImageSize.width())).arg(static_cast<int>(m_sourceImageSize.height()));
        m_imageSizeItem->setPlainText(txt);
        // 将文本放在 image 的左上角（图像坐标 (5,5) 映射到 scene）
        QPointF pos = mapImageToScene(QPointF(5.0, 5.0));
        m_imageSizeItem->setPos(pos);
        m_imageSizeItem->setVisible(true);
    } else {
        m_imageSizeItem->setVisible(false);
    }
}

// 当图像被清空时隐藏图片尺寸显示（修改）
void MatchScene::onImageCleared()
{
    ImageSceneBase::onImageCleared();
    if (m_imageSizeItem) m_imageSizeItem->setVisible(false);
}

void MatchScene::setCompositionMode(MatchScene::CompositionMode m)
{
    m_compMode = m;
    emit roiChanged();
}

void MatchScene::setShapeMode(MatchScene::ShapeMode s)
{
    m_shapeMode = s;
}

void MatchScene::setPenWidth(int w)
{
    m_penWidth = std::max(1, w);
}

void MatchScene::setSelectionTemplateEnabled(bool enable)
{
    m_selectionTemplateEnabled = enable;
}

void MatchScene::setRoiAreaSelectionEnabled(bool enable)
{
    m_RoiAreaSelectionEnabled = enable;
    if (m_RoiAreaSelectionEnabled) {
        if (!m_matchAreaItem) {
            QPen pen(QColor(255,165,0));
            pen.setWidthF(1.2);
            pen.setStyle(Qt::DashLine);
            m_matchAreaItem = addRect(QRectF(), pen, QColor(255,165,0,40)); // 在场景中添加矩形项用于显示 match-area（初始为空）
            if (m_matchAreaItem) {
                m_matchAreaItem->setZValue(4.0);
                m_matchAreaItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
                m_matchAreaItem->setFlag(QGraphicsItem::ItemIsMovable, false);
                m_matchAreaItem->setAcceptedMouseButtons(Qt::NoButton);
                m_matchAreaItem->setVisible(false);
            }
        }
        if (!m_rotateHandleItem) {
            QPen rpen(Qt::red); // 旋转把手使用红色
            rpen.setWidthF(5.0);
            m_rotateHandleItem = addRect(QRectF(), rpen, QBrush(QColor(200,0,0))); // 使用小矩形作为旋转把手
            if (m_rotateHandleItem) {
                m_rotateHandleItem->setZValue(6.0); // 放在较高层级
                m_rotateHandleItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
                m_rotateHandleItem->setFlag(QGraphicsItem::ItemIsMovable, false);
                m_rotateHandleItem->setAcceptedMouseButtons(Qt::NoButton);
                m_rotateHandleItem->setVisible(false);
            }
        }
        // 创建四个角把手（小红色矩形）
        for (int i = 0; i < 4; ++i) {
            if (!m_cornerHandleItems[i]) {
                QPen cpen(Qt::red);
                cpen.setWidthF(5.0);
                m_cornerHandleItems[i] = addRect(QRectF(), cpen, QBrush(QColor(200,0,0))); // 红色小矩形把手
                if (m_cornerHandleItems[i]) {
                    m_cornerHandleItems[i]->setZValue(6.0);
                    m_cornerHandleItems[i]->setFlag(QGraphicsItem::ItemIsSelectable, false);
                    m_cornerHandleItems[i]->setFlag(QGraphicsItem::ItemIsMovable, false);
                    m_cornerHandleItems[i]->setAcceptedMouseButtons(Qt::NoButton);
                    m_cornerHandleItems[i]->setVisible(false);
                }
            }
        }
    } else {
        //if (m_matchAreaItem) m_matchAreaItem->setVisible(false);
        if (m_rotateHandleItem) m_rotateHandleItem->setVisible(false);
        for (int i=0;i<4;++i) if (m_cornerHandleItems[i]) m_cornerHandleItems[i]->setVisible(false);
    }
}

QPointF MatchScene::imageToScenePoint(const QPointF &pt) const
{
    return mapImageToScene(pt);
}

QPointF MatchScene::sceneToImagePoint(const QPointF &pt) const
{
    return mapSceneToImage(pt);
}

void MatchScene::clear()
{
    clearOverlays();
    m_totalPath = QPainterPath();
    if (m_totalPathItem) {
        removeItem(m_totalPathItem);
        delete m_totalPathItem;
        m_totalPathItem = nullptr;
    }
    if(m_matchAreaItem) {
        m_matchAreaItem->setVisible(false);
    }
    if(m_rotateHandleItem) {
        m_rotateHandleItem->setVisible(false);
    }
    for(int i=0;i<4;++i) {
        if(m_cornerHandleItems[i]) {
            m_cornerHandleItems[i]->setVisible(false);
        }
    }
    m_matchSceneRect = QRectF();
    emit roiChanged();
    if(!m_totalPath.isEmpty() && !getSourceImageMat().isNull())
    {
        emit totalPathPathChanged(m_totalPath);
    }

    QList<QGraphicsItem*> allItems = items();
    for(QGraphicsItem* item : allItems) {
        if(item->type() == InteractivePathItem::Type) {
            removeItem(item); 
            delete item;
        }
    }

}
    

void MatchScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) 
    {
        const QPointF imgPt = mapSceneToImage(event->scenePos());
        m_currentClickPos = imgPt;
        emit currentPointChanged(m_currentClickPos);
    
    if (m_RoiAreaSelectionEnabled) 
    {
            QPointF p = event->scenePos();
            if (!m_matchAreaItem) setRoiAreaSelectionEnabled(true);

            const double handleRadius = 8.0; // 旋转把手命中检测的半径（像素）
            if (m_rotateHandleItem && m_rotateHandleItem->isVisible()) 
            {
                QPointF handleCenter = m_rotateHandleItem->rect().center();
                QPointF hc = m_rotateHandleItem->sceneBoundingRect().center();
                if (QLineF(hc, p).length() <= handleRadius) 
                {
                    m_matchRotating = true; // 标记当前为旋转交互
                    m_matchPressPos = p; // 记录按下位置用于后续角度计算
                    event->accept();
                    return;
                }
            }

            if (m_matchAreaItem && m_matchAreaItem->isVisible()) 
            {
                QRectF r = m_matchAreaItem->rect();
                QRectF sceneRect = m_matchAreaItem->mapToScene(r).boundingRect();
                const double cornerRadius = 20.0; // 角点命中半径（像素）- 扩大一倍
                // 优先使用可视化角点把手检测命中
                for (int i = 0; i < 4; ++i) 
                {
                    if (m_cornerHandleItems[i] && m_cornerHandleItems[i]->isVisible()) 
                    {
                        QPointF chc = m_cornerHandleItems[i]->sceneBoundingRect().center();
                        if (QLineF(chc, p).length() <= cornerRadius) 
                        {
                            m_matchResizing = true;
                            m_matchPressPos = p;
                            switch (i) {
                                case 0: m_matchResizeCorner = MR_TopLeft; break;
                                case 1: m_matchResizeCorner = MR_TopRight; break;
                                case 2: m_matchResizeCorner = MR_BottomLeft; break;
                                case 3: m_matchResizeCorner = MR_BottomRight; break;
                            }
                            event->accept();
                            return;
                        }
                    }
                }

                if (sceneRect.contains(p)) 
                { 
                    m_matchDragging = true; 
                    m_matchResizeCorner = MR_Move; 
                    m_matchPressPos = p; 
                    event->accept(); 
                    return; 
                }
            }

            m_matchDragging = true; // 开始以拖动方式创建新矩形
            m_matchPressPos = event->scenePos(); // 记录起始点（场景坐标）
            m_matchSceneRect = QRectF(m_matchPressPos, QSizeF()); // 初始化场景坐标下的矩形（起点，大小为0）
            if (!m_matchAreaItem) setRoiAreaSelectionEnabled(true);
            if (m_matchAreaItem) {
                m_matchAreaItem->setRect(m_matchSceneRect); // 在 item 本地坐标设置矩形边界
                m_matchAreaItem->setTransformOriginPoint(m_matchAreaItem->rect().center()); // 将变换原点设为矩形中心（局部坐标），以便之后旋转围绕中心
                m_matchAreaItem->setRotation(0.0); // 清除任何残留旋转，初始化为 0
                m_matchAreaItem->setVisible(true);
            }
            if (m_rotateHandleItem) {
                m_rotateHandleItem->setVisible(false);
            }
            for (int i=0;i<4;++i) if (m_cornerHandleItems[i]) m_cornerHandleItems[i]->setVisible(false);
            event->accept();
            return;
        }


    if (!m_selectionTemplateEnabled) {
        QGraphicsScene::mousePressEvent(event);
        return; 
    }
    m_drawing = true;
    m_pressPos = event->scenePos();

    if (m_shapeMode == Free) {
        m_currentPath = QPainterPath(m_pressPos); 
        if (!m_previewPathItem) {
            QPen pen(Qt::yellow);
            pen.setWidth(2);
            m_previewPathItem = addPath(m_currentPath, pen);
            m_previewPathItem->setZValue(5);
        } else {
            m_previewPathItem->setPath(m_currentPath);
            m_previewPathItem->setVisible(true);
        }
    } else {
        m_tempRect = QRectF(m_pressPos, QSizeF());
        if (m_shapeMode == Rect) {
            if (!m_previewRectItem) {
                QPen pen(Qt::yellow);
                pen.setWidth(2);
                m_previewRectItem = addRect(m_tempRect, pen, QBrush(Qt::NoBrush)); 
                m_previewRectItem->setZValue(5);
            } else {
                m_previewRectItem->setRect(m_tempRect); 
                m_previewRectItem->setVisible(true);
            }
        } else if (m_shapeMode == Ellipse) {
            if (!m_previewEllipseItem) {
                QPen pen(Qt::yellow);
                pen.setWidth(2);
                m_previewEllipseItem = addEllipse(m_tempRect, pen, QBrush(Qt::NoBrush)); 
                m_previewEllipseItem->setZValue(5);
            } else {
                m_previewEllipseItem->setRect(m_tempRect);
                m_previewEllipseItem->setVisible(true);
            }
        }
    }
    }
    QGraphicsScene::mousePressEvent(event);
}

void MatchScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_RoiAreaSelectionEnabled && (m_matchDragging || m_matchResizing || m_matchRotating)) {
        QPointF p = event->scenePos(); // 当前鼠标位置（场景坐标）
        if (m_matchDragging && m_matchResizeCorner == MR_Move) {
            QPointF delta = p - m_matchPressPos;
            m_matchPressPos = p;
            m_matchSceneRect.translate(delta);
            if (m_matchAreaItem) {
                m_matchAreaItem->setRect(m_matchSceneRect); // 移动时更新矩形的本地边界
                m_matchAreaItem->setTransformOriginPoint(m_matchAreaItem->rect().center()); // 更新变换原点以保持中心为旋转中心
            }
        } else if (m_matchDragging && m_matchResizeCorner == MR_None) {
            m_matchSceneRect = QRectF(m_matchPressPos, p).normalized();
            if (m_matchAreaItem) {
                m_matchAreaItem->setRect(m_matchSceneRect); // 绘制新矩形时更新显示
                m_matchAreaItem->setTransformOriginPoint(m_matchAreaItem->rect().center()); // 并设置变换原点为中心
            }
        } else if (m_matchResizing) {
            QRectF r = m_matchSceneRect;
            // 修复缩放逻辑：使用 mapFromScene 将当前鼠标位置转换到 item 本地坐标系
            // 这样可以避免因旋转导致的坐标系不一致问题，确保缩放点跟随鼠标
            QPointF localP = m_matchAreaItem ? m_matchAreaItem->mapFromScene(p) : p;
            
            switch (m_matchResizeCorner) {
            case MR_TopLeft: r.setTopLeft(localP); break;
            case MR_TopRight: r.setTopRight(localP); break;
            case MR_BottomLeft: r.setBottomLeft(localP); break;
            case MR_BottomRight: r.setBottomRight(localP); break;
            default: break;
            }
            m_matchSceneRect = r.normalized();
            if (m_matchAreaItem) {
                m_matchAreaItem->setRect(m_matchSceneRect); // 缩放后更新 item 的本地矩形
                m_matchAreaItem->setTransformOriginPoint(m_matchAreaItem->rect().center()); // 更新变换原点以保持中心为旋转中心
            }
        } else if (m_matchRotating) {
            QPointF center = m_matchAreaItem ? m_matchAreaItem->mapToScene(m_matchAreaItem->rect().center()) : QPointF(); // 使用矩形中心（映射至场景坐标）作为旋转计算的参考点
            QPointF v1 = m_matchPressPos - center;
            QPointF v2 = p - center;
            double a1 = std::atan2(v1.y(), v1.x());
            double a2 = std::atan2(v2.y(), v2.x());
            const double PI = 3.14159265358979323846;
            double angle = (a2 - a1) * 180.0 / PI;
            m_matchRotation += angle;
            if (m_matchAreaItem) {
                m_matchAreaItem->setTransformOriginPoint(m_matchAreaItem->rect().center()); // 在应用旋转前保证变换原点为本地中心
                m_matchAreaItem->setRotation(m_matchRotation); // 应用累计旋转角度到 item
            }
            m_matchPressPos = p;
        }

        if (m_matchAreaItem && m_rotateHandleItem) {
            QRectF mapped = m_matchAreaItem->mapToScene(m_matchAreaItem->rect()).boundingRect(); // 将本地矩形映射为场景包围盒（用于计算）
            QPointF sceneTL = m_matchAreaItem->mapToScene(m_matchAreaItem->rect().topLeft()); // 本地左上角映射到场景坐标
            QPointF sceneTR = m_matchAreaItem->mapToScene(m_matchAreaItem->rect().topRight()); // 本地右上角映射到场景坐标
            QPointF sceneBL = m_matchAreaItem->mapToScene(m_matchAreaItem->rect().bottomLeft()); // 本地左下角映射到场景坐标
            QPointF sceneBR = m_matchAreaItem->mapToScene(m_matchAreaItem->rect().bottomRight()); // 本地右下角映射到场景坐标
            QPointF topCenter = (sceneTL + sceneTR) / 2.0; // 顶边中点（场景坐标）
            QPointF handleCenter = topCenter + QPointF(0, -12);
            const double hs = 10.0;
            m_rotateHandleItem->setRect(QRectF(handleCenter.x()-hs/2, handleCenter.y()-hs/2, hs, hs));
            m_rotateHandleItem->setVisible(!m_matchSceneRect.isNull());
            const double chs = 16.0; // 角把手大小 - 扩大一倍 (原为 8.0)
            if (m_cornerHandleItems[0]) m_cornerHandleItems[0]->setRect(QRectF(sceneTL.x()-chs/2, sceneTL.y()-chs/2, chs, chs));
            if (m_cornerHandleItems[1]) m_cornerHandleItems[1]->setRect(QRectF(sceneTR.x()-chs/2, sceneTR.y()-chs/2, chs, chs));
            if (m_cornerHandleItems[2]) m_cornerHandleItems[2]->setRect(QRectF(sceneBL.x()-chs/2, sceneBL.y()-chs/2, chs, chs));
            if (m_cornerHandleItems[3]) m_cornerHandleItems[3]->setRect(QRectF(sceneBR.x()-chs/2, sceneBR.y()-chs/2, chs, chs));
            for (int i=0;i<4;++i) if (m_cornerHandleItems[i]) m_cornerHandleItems[i]->setVisible(!m_matchSceneRect.isNull());
        }
        event->accept();
        return;
    }

    if (!m_drawing) {
        QGraphicsScene::mouseMoveEvent(event);
        return;
    }
    QPointF p = event->scenePos();
    if (m_shapeMode == Free) {
        m_currentPath.lineTo(p);
        if (m_previewPathItem) m_previewPathItem->setPath(m_currentPath);
    } else {
        m_tempRect = QRectF(m_pressPos, p).normalized();
        if (m_shapeMode == Rect && m_previewRectItem) {
            m_previewRectItem->setRect(m_tempRect);
        } else if (m_shapeMode == Ellipse && m_previewEllipseItem) {
            m_previewEllipseItem->setRect(m_tempRect);
        }
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void MatchScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_RoiAreaSelectionEnabled && (m_matchDragging || m_matchResizing || m_matchRotating)) {
        m_matchDragging = false;
        m_matchResizing = false;
        m_matchRotating = false;
        m_matchResizeCorner = MR_None;
        if (!m_matchSceneRect.isNull() && m_sourceImageSize.isValid()) {
            QRectF localRect = m_matchAreaItem->rect(); // 获取矩形在 item 本地坐标系中的边界
            QPointF sceneTL = m_matchAreaItem->mapToScene(localRect.topLeft()); // 本地左上角映射到场景坐标
            QPointF sceneTR = m_matchAreaItem->mapToScene(localRect.topRight()); // 本地右上角映射到场景坐标
            QPointF sceneBL = m_matchAreaItem->mapToScene(localRect.bottomLeft()); // 本地左下角映射到场景坐标
            QPointF sceneBR = m_matchAreaItem->mapToScene(localRect.bottomRight()); // 本地右下角映射到场景坐标
            QPointF sceneCenter = m_matchAreaItem->mapToScene(localRect.center()); // 本地中心映射到场景坐标
            QPointF sceneLeftMid = (sceneTL + sceneBL) / 2.0; // 左边中点（场景坐标）
            QPointF sceneRightMid = (sceneTR + sceneBR) / 2.0; // 右边中点
            QPointF sceneTopMid = (sceneTL + sceneTR) / 2.0; // 顶边中点
            QPointF sceneBottomMid = (sceneBL + sceneBR) / 2.0; // 底边中点
            QPointF imgCenter = mapSceneToImage(sceneCenter); // 将场景中心映射为图像像素坐标
            QPointF imgLeft = mapSceneToImage(sceneLeftMid); // 左边中点映射为图像坐标
            QPointF imgRight = mapSceneToImage(sceneRightMid); // 右边中点映射为图像坐标
            double imgW = QLineF(imgLeft, imgRight).length(); // 计算图像空间中的宽度（像素）
            QPointF imgTop = mapSceneToImage(sceneTopMid); // 顶边中点映射为图像坐标
            QPointF imgBottom = mapSceneToImage(sceneBottomMid); // 底边中点映射为图像坐标
            double imgH = QLineF(imgTop, imgBottom).length(); // 计算图像空间中的高度（像素）

            double angleRad = m_matchRotation * (3.14159265358979323846/180.0);
            double cosA = std::cos(angleRad), sinA = std::sin(angleRad);
            double halfW = imgW/2.0, halfH = imgH/2.0;
            std::array<QPointF,4> corners = {
                QPointF(-halfW, -halfH), QPointF(halfW, -halfH), QPointF(halfW, halfH), QPointF(-halfW, halfH)
            };
            double minx=1e12, miny=1e12, maxx=-1e12, maxy=-1e12;
            for (auto &c: corners) {
                double rx = c.x()*cosA - c.y()*sinA + imgCenter.x(); // 将本地角点按旋转矩阵变换后平移到图像中心，得到全局图像坐标 x
                double ry = c.x()*sinA + c.y()*cosA + imgCenter.y(); // 得到全局图像坐标 y
                minx = std::min(minx, rx); miny = std::min(miny, ry); // 更新包围盒最小坐标
                maxx = std::max(maxx, rx); maxy = std::max(maxy, ry); // 更新包围盒最大坐标
            }
            int ix = static_cast<int>(std::floor(minx));
            int iy = static_cast<int>(std::floor(miny));
            int iw = static_cast<int>(std::ceil(maxx)) - ix;
            int ih = static_cast<int>(std::ceil(maxy)) - iy;
            ix = std::clamp(ix, 0, static_cast<int>(m_sourceImageSize.width()));
            iy = std::clamp(iy, 0, static_cast<int>(m_sourceImageSize.height()));
            iw = std::clamp(iw, 0, static_cast<int>(m_sourceImageSize.width()-ix));
            ih = std::clamp(ih, 0, static_cast<int>(m_sourceImageSize.height()-iy));
            const_cast<MatchScene*>(this)->m_lastMatchBbox = cv::Rect(ix, iy, iw, ih);
        }
        event->accept();
        return;
    }


    if (!m_drawing) {
        QGraphicsScene::mouseReleaseEvent(event);
        return;
    }
    m_drawing = false;

    QPainterPath newPath;
    if (m_shapeMode == Free) {
        newPath = m_currentPath;
        QPainterPathStroker stroker;// 填充路径
        stroker.setWidth(m_penWidth);
        newPath = stroker.createStroke(newPath); 
    } else if (m_shapeMode == Rect) {
        newPath.addRect(m_tempRect);
    } else if (m_shapeMode == Ellipse) {
        newPath.addEllipse(m_tempRect);
    }

    if (m_compMode == United) {
        m_totalPath = m_totalPath.united(newPath);
    } else if (m_compMode == Intersected) {
        if (m_totalPath.isEmpty()) m_totalPath = newPath;
        else m_totalPath = m_totalPath.intersected(newPath);
    } else if (m_compMode == Subtracted) {
        m_totalPath = m_totalPath.subtracted(newPath);
    }

    if (m_previewPathItem) {
        if (items().contains(m_previewPathItem)) removeItem(m_previewPathItem);
        delete m_previewPathItem;
        m_previewPathItem = nullptr;
    }
    if (m_previewRectItem) {
        if (items().contains(m_previewRectItem)) removeItem(m_previewRectItem);
        delete m_previewRectItem;
        m_previewRectItem = nullptr;
    }
    if (m_previewEllipseItem) {
        if (items().contains(m_previewEllipseItem)) removeItem(m_previewEllipseItem);
        delete m_previewEllipseItem;
        m_previewEllipseItem = nullptr;
    }

    if (!m_totalPath.isEmpty()) {
        QPen pen(Qt::yellow);
        pen.setWidth(2);
        QBrush brush(QColor(255, 255, 0, 64));
        if (!m_totalPathItem) {
            m_totalPathItem = addPath(m_totalPath, pen, brush);
            if (m_totalPathItem) {
                m_totalPathItem->setZValue(5);
            }
        } else {
            m_totalPathItem->setPath(m_totalPath);
            m_totalPathItem->setVisible(true);
            m_totalPathItem->setPen(pen);
            m_totalPathItem->setBrush(brush);
        }
    } else {
        if (m_totalPathItem) {
            if (items().contains(m_totalPathItem)) removeItem(m_totalPathItem);
            delete m_totalPathItem;
            m_totalPathItem = nullptr;
        }
    }
    emit roiChanged();
    if(!m_totalPath.isEmpty() && !getSourceImageMat().isNull())
    {
        emit totalPathPathChanged(m_totalPath);
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

// 绘制总路径集合，并在场景中生成可交互的路径项
void MatchScene::DrawTotalPath(QVector<QPainterPath> paths)
{
    clear();
    drawPaths = paths; 
    for(const QPainterPath& p : paths) {
        if(p.isEmpty()) continue;
        InteractivePathItem* item = new InteractivePathItem(p);
        item->setBrush(Qt::NoBrush);
        addItem(item);
    }
}

static cv::Mat qImageToMatRGBA(const QImage &in)
{
    QImage img = in.convertToFormat(QImage::Format_RGBA8888); // 转为 RGBA 格式，保证内存布局
    cv::Mat mat(img.height(), img.width(), CV_8UC4, const_cast<uchar*>(img.bits()), img.bytesPerLine()); // 创建不拷贝的 Mat
    return mat.clone(); // 返回深拷贝，避免 QImage 生命周期问题
}

static cv::Mat qImageToMatGray(const QImage &in)
{
    QImage img = in.convertToFormat(QImage::Format_Grayscale8); // 转为灰度格式
    cv::Mat mat(img.height(), img.width(), CV_8UC1, const_cast<uchar*>(img.bits()), img.bytesPerLine()); // 使用 QImage 内存
    return mat.clone(); // 深拷贝返回
}

cv::Mat MatchScene::extractTemplate() const
{
    QImage src = getSourceImageMat();
    if (src.isNull()) return cv::Mat();

    QSize sz = src.size();
    QImage mask(sz, QImage::Format_Grayscale8);
    mask.fill(Qt::black);

    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing);
    QBrush brush(Qt::white);
    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);

    QPainterPath finalPath = m_totalPath; // 获取合成路径
    painter.drawPath(finalPath); // 在掩码上绘制路径（白色区域代表模板）
    painter.end();

    cv::Mat srcMat = qImageToMatRGBA(src); // 将源图像转为 RGBA cv::Mat
    cv::Mat maskMat = qImageToMatGray(mask); // 将掩码转为灰度 Mat

    if (srcMat.empty() || maskMat.empty()) return cv::Mat();

    cv::Mat bgr;
    cv::cvtColor(srcMat, bgr, cv::COLOR_RGBA2BGR); // RGBA -> BGR

    // 修改逻辑：不再使用 copyTo 把背景涂黑，而是构建 BGRA 图像
    // RGB 通道保留原图（包含背景），Alpha 通道存放掩膜
    std::vector<cv::Mat> channels;
    cv::split(bgr, channels);
    channels.push_back(maskMat); // 添加掩膜作为 Alpha 通道
    cv::Mat bgra;
    cv::merge(channels, bgra);

    cv::Rect bbox;
    cv::Mat nonzero; // 非零像素点集合
    cv::findNonZero(maskMat, nonzero); // 找到掩码中的白色像素
    if (nonzero.total() == 0) {
        return cv::Mat();
    }
    bbox = cv::boundingRect(nonzero);
    cv::Rect bounds(0, 0, bgr.cols, bgr.rows);
    bbox &= bounds; //  再次将包围盒裁剪到图像范围
    if (bbox.width <= 0 || bbox.height <= 0) {
        qWarning().noquote() << QStringLiteral("extractTemplate 生成的包围盒无效，已跳过裁剪：bbox=%1,%2,%3,%4")
                                .arg(bbox.x).arg(bbox.y).arg(bbox.width).arg(bbox.height);
        return cv::Mat();
    }
    const_cast<MatchScene*>(this)->m_lastExtractBbox = bbox; // 保存供后续查询
    
    // 裁剪 BGRA 图像，保留了原图背景信息和 Alpha 掩膜
    cv::Mat cropped = bgra(bbox).clone();
    qDebug() << "Extracted src size:" << srcMat.cols << "x" << srcMat.rows;
    qDebug() << "Extracted template size:" << cropped.cols << "x" << cropped.rows;
    return cropped;
}

cv::Mat MatchScene::extractMatchAreaMask() const
{
    QImage src = getSourceImageMat();
    if (src.isNull()) return cv::Mat();

    if (m_lastMatchBbox.width == 0 || m_lastMatchBbox.height == 0) return cv::Mat();

    cv::Mat srcMat = qImageToMatRGBA(src);
    if (srcMat.empty()) return cv::Mat();

    cv::Mat bgr;
    cv::cvtColor(srcMat, bgr, cv::COLOR_RGBA2BGR);

    double cx = m_lastMatchBbox.x + m_lastMatchBbox.width / 2.0; // 计算像素包围盒中心 x
    double cy = m_lastMatchBbox.y + m_lastMatchBbox.height / 2.0; // 计算像素包围盒中心 y
    const double angle = -m_matchRotation; // 取负角度来将选区对齐到轴向
    cv::Mat rot = cv::getRotationMatrix2D(cv::Point2f(static_cast<float>(cx), static_cast<float>(cy)), static_cast<float>(angle), 1.0); // 生成以 cx,cy 为中心的仿射旋转矩阵
    cv::Mat rotated;
    cv::warpAffine(bgr, rotated, rot, bgr.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0,0,0)); // 对整张图像做旋转，边界填充为黑色

    int x = static_cast<int>(std::round(cx - m_lastMatchBbox.width / 2.0)); // 将中心回推到矩形左上角 x
    int y = static_cast<int>(std::round(cy - m_lastMatchBbox.height / 2.0)); // 左上角 y
    x = std::clamp(x, 0, rotated.cols); // 限定在图像范围内
    y = std::clamp(y, 0, rotated.rows);
    int w = std::max(0, std::min(m_lastMatchBbox.width, rotated.cols - x)); // 计算有效宽度（避免越界）
    int h = std::max(0, std::min(m_lastMatchBbox.height, rotated.rows - y)); // 计算有效高度
    if (w == 0 || h == 0) return cv::Mat(); // 空区域直接返回空 Mat
    cv::Mat mask = cv::Mat::zeros(srcMat.size(), CV_8UC1); // 创建与旋转后图像同尺寸的黑色掩码
    cv::Rect roi(x, y, w, h); // 构造裁剪矩形
    cv::Rect roiBounds(0, 0, mask.cols, mask.rows);
    roi &= roiBounds; //  确保写入掩膜的矩形完全落在 mask 内部
    if (roi.width <= 0 || roi.height <= 0) {
        qWarning().noquote() << QStringLiteral("extractMatchAreaMask 生成的 ROI 越界，已忽略：roi=%1,%2,%3,%4")
                                .arg(roi.x).arg(roi.y).arg(roi.width).arg(roi.height);
        return cv::Mat();
    }
    mask(roi).setTo(255); // 在掩码上将目标区域设为白色
    return mask;
}

QRect MatchScene::getMatchAreaBoundingRect() const
{
    if (m_lastMatchBbox.width == 0 || m_lastMatchBbox.height == 0) return QRect();
    return QRect(m_lastMatchBbox.x, m_lastMatchBbox.y, m_lastMatchBbox.width, m_lastMatchBbox.height);
}

QRect MatchScene::getTemplateBoundingRect() const
{
    if (m_lastExtractBbox.width == 0 || m_lastExtractBbox.height == 0) return QRect(); // 无效包围盒返回空
    return QRect(m_lastExtractBbox.x, m_lastExtractBbox.y, m_lastExtractBbox.width, m_lastExtractBbox.height); // 转换为 QRect 返回
}

