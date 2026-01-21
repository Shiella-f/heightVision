#pragma once
#include <QWidget>
#include <QMouseEvent>
#include <QCursor>

class baseWidget : public QWidget {
    Q_OBJECT

public:
    baseWidget(QWidget* parent = nullptr); 

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void childEvent(QChildEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

protected:
    void checkResizeDirection(const QPoint& pos);
    void updateCursorShape(const QPoint& pos);
private:
    QRect m_startGeometry;
    QPoint m_pressPos;
    bool m_isResize = false;
    enum ResizeDirection {
        NoResize,
        ResizeLeft,
        ResizeRight,
        ResizeTop,
        ResizeBottom,
        ResizeTopLeft,
        ResizeTopRight,
        ResizeBottomLeft,
        ResizeBottomRight
    };
    ResizeDirection m_resizeDirection = NoResize;
   
};