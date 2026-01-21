#include "CustomTitleBar.h"
#include <QHBoxLayout>
#include <QStyle>

CustomTitleBar::CustomTitleBar(QWidget *parent) : QWidget(parent) {
    setFixedHeight(30);
    // 样式将通过 QSS 文件统一管理，这里设置默认样式以防万一
    setStyleSheet("background-color: #3c3c3c;");

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("color: #cccccc; font-family: 'Segoe UI'; font-size: 12px; padding-left: 10px;");

    m_minimizeBtn = new QPushButton("—", this); // 使用 em dash 或类似符号
    m_minimizeBtn->setFixedSize(45, 30);
    m_minimizeBtn->setObjectName("titleBarMinBtn");
    m_minimizeBtn->setStyleSheet("QPushButton { border: none; color: #cccccc; background-color: transparent; }"
                                 "QPushButton:hover { background-color: #505050; }");
    connect(m_minimizeBtn, &QPushButton::clicked, this, &CustomTitleBar::minimizeClicked);

    m_closeBtn = new QPushButton("✕", this); // 使用乘号或类似符号
    m_closeBtn->setFixedSize(45, 30);
    m_closeBtn->setObjectName("titleBarCloseBtn");
    m_closeBtn->setStyleSheet("QPushButton { border: none; color: #cccccc; background-color: transparent; }"
                              "QPushButton:hover { background-color: #e81123; color: white; }");
    connect(m_closeBtn, &QPushButton::clicked, this, &CustomTitleBar::closeClicked);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_titleLabel);
    layout->addStretch();
    layout->addWidget(m_minimizeBtn);
    layout->addWidget(m_closeBtn);
}

void CustomTitleBar::setTitle(const QString& title) {
    m_titleLabel->setText(title);
}

void CustomTitleBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isPressed = true;
        m_dragPosition = event->globalPos() - window()->frameGeometry().topLeft();
        event->accept();
    }
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_isPressed && (event->buttons() & Qt::LeftButton)) {
        window()->move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}

void CustomTitleBar::mouseReleaseEvent(QMouseEvent *event) {
    m_isPressed = false;
}
