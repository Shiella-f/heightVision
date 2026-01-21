#include"baseWidget.h"
#include <QChildEvent>

baseWidget::baseWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    installEventFilter(this);
}

void baseWidget::childEvent(QChildEvent *event)
{
    if (event->type() == QEvent::ChildAdded) {
        if (event->child()->isWidgetType()) {
            event->child()->installEventFilter(this);
            QWidget* widget = static_cast<QWidget*>(event->child());
            widget->setMouseTracking(true);
        }
    }
}

bool baseWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint globalPos = mouseEvent->globalPos();
        QPoint localPos = this->mapFromGlobal(globalPos);
        
        if (m_isResize) {
            QPoint delta = globalPos - m_pressPos;
            QRect newGeom = m_startGeometry;
            switch(m_resizeDirection) {
                case ResizeLeft: newGeom.setLeft(m_startGeometry.left() + delta.x()); break;
                case ResizeRight: newGeom.setRight(m_startGeometry.right() + delta.x()); break;
                case ResizeTop: newGeom.setTop(m_startGeometry.top() + delta.y()); break;
                case ResizeBottom: newGeom.setBottom(m_startGeometry.bottom() + delta.y()); break;
                case ResizeTopLeft: newGeom.setTopLeft(m_startGeometry.topLeft() + delta); break;
                case ResizeTopRight: newGeom.setTopRight(m_startGeometry.topRight() + delta); break;
                case ResizeBottomLeft: newGeom.setBottomLeft(m_startGeometry.bottomLeft() + delta); break;
                case ResizeBottomRight: newGeom.setBottomRight(m_startGeometry.bottomRight() + delta); break;
                default: break;
            }
            this->setGeometry(newGeom);
            return true;
        } else {
            updateCursorShape(localPos);
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QPoint localPos = this->mapFromGlobal(mouseEvent->globalPos());
            checkResizeDirection(localPos);
            if (m_resizeDirection != NoResize) {
                m_isResize = true;
                m_pressPos = mouseEvent->globalPos();
                m_startGeometry = this->geometry();
                return true;
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (m_isResize) {
            m_isResize = false;
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}


void baseWidget::mousePressEvent(QMouseEvent *event) 
{
    if(event->button() == Qt::LeftButton ){
        m_pressPos = event->globalPos();
        m_startGeometry = this->geometry();
        checkResizeDirection(event->pos());
        m_isResize = (m_resizeDirection != NoResize);
    }
    QWidget::mousePressEvent(event);
}
void baseWidget::mouseMoveEvent(QMouseEvent *event) 
{
    if(m_isResize) {
        QPoint delta = event->globalPos() - m_pressPos;
        QRect newGeom = m_startGeometry;
        switch(m_resizeDirection) {
            case ResizeLeft:
                newGeom.setLeft(m_startGeometry.left() + delta.x());
                break;
            case ResizeRight:
                newGeom.setRight(m_startGeometry.right() + delta.x());
                break;
            case ResizeTop:
                newGeom.setTop(m_startGeometry.top() + delta.y());
                break;
            case ResizeBottom:
                newGeom.setBottom(m_startGeometry.bottom() + delta.y());
                break;
            case ResizeTopLeft:
                newGeom.setTopLeft(m_startGeometry.topLeft() + delta);
                break;
            case ResizeTopRight:
                newGeom.setTopRight(m_startGeometry.topRight() + delta);
                break;
            case ResizeBottomLeft:
                newGeom.setBottomLeft(m_startGeometry.bottomLeft() + delta);
                break;
            case ResizeBottomRight:
                newGeom.setBottomRight(m_startGeometry.bottomRight() + delta);
                break;
            default:
                break;
        }
        this->setGeometry(newGeom);
        event->accept();
    } else {
        updateCursorShape(event->pos()); // 只有在非调整大小时才更新光标形状
    }
    QWidget::mouseMoveEvent(event);
}
void baseWidget::mouseReleaseEvent(QMouseEvent *event) 
{
if (event->button() == Qt::LeftButton)
    {
        m_isResize = false;
        event->accept();
    }
    QWidget::mouseReleaseEvent(event);
}

void baseWidget::checkResizeDirection(const QPoint& pos) {
    const int resizeMargin = 5; // 调整边距大小
    QRect rect = this->rect();
    m_resizeDirection = NoResize;
    if(pos.x() <= resizeMargin && pos.y() <= resizeMargin) {
        m_resizeDirection = ResizeTopLeft;
    } else if(pos.x() >= rect.width() - resizeMargin && pos.y() <= resizeMargin) {
        m_resizeDirection = ResizeTopRight;
    } else if(pos.x() <= resizeMargin && pos.y() >= rect.height() - resizeMargin) {
        m_resizeDirection = ResizeBottomLeft;
    } else if(pos.x() >= rect.width() - resizeMargin && pos.y() >= rect.height() - resizeMargin) {
        m_resizeDirection = ResizeBottomRight;
    } else if(pos.x() <= resizeMargin) {
        m_resizeDirection = ResizeLeft;
    } else if(pos.x() >= rect.width() - resizeMargin) {
        m_resizeDirection = ResizeRight;
    } else if(pos.y() <= resizeMargin) {
        m_resizeDirection = ResizeTop;
    } else if(pos.y() >= rect.height() - resizeMargin) {
        m_resizeDirection = ResizeBottom;
    }
}

void baseWidget::updateCursorShape(const QPoint& pos) {
    const int resizeMargin = 5;
    QRect rect = this->rect();
    Qt::CursorShape cursor = Qt::ArrowCursor;
    if(pos.x() <= resizeMargin && pos.y() <= resizeMargin) {
        cursor = Qt::SizeFDiagCursor;
    } else if(pos.x() >= rect.width() - resizeMargin && pos.y() <= resizeMargin) {
        cursor = Qt::SizeBDiagCursor;
    } else if(pos.x() <= resizeMargin && pos.y() >= rect.height() - resizeMargin) {
        cursor = Qt::SizeBDiagCursor;
    } else if(pos.x() >= rect.width() - resizeMargin && pos.y() >= rect.height() - resizeMargin) {
        cursor = Qt::SizeFDiagCursor;
    } else if(pos.x() <= resizeMargin) {
        cursor = Qt::SizeHorCursor;
    } else if(pos.x() >= rect.width() - resizeMargin) {
        cursor = Qt::SizeHorCursor;
    } else if(pos.y() <= resizeMargin) {
        cursor = Qt::SizeVerCursor;
    } else if(pos.y() >= rect.height() - resizeMargin) {
        cursor = Qt::SizeVerCursor;
    }
    
    if (cursor != Qt::ArrowCursor) {
        setCursor(cursor);
    } else {
        unsetCursor();
    }
}

void baseWidget::leaveEvent(QEvent *event) 
{
    unsetCursor();
    m_resizeDirection = NoResize; // 清空方向
    QWidget::leaveEvent(event);
}
    