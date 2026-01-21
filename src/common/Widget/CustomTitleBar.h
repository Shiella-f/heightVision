#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>

class CustomTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit CustomTitleBar(QWidget *parent = nullptr);
    void setTitle(const QString& title);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void minimizeClicked();
    void closeClicked();

private:
    QLabel *m_titleLabel;
    QPushButton *m_minimizeBtn;
    QPushButton *m_closeBtn;
    QPoint m_dragPosition;
    bool m_isPressed = false;
};
