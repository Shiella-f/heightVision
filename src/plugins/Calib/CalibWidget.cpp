#include "CalibWidget.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include "../../common/Widget/CustomTitleBar.h"

CalibWidget::CalibWidget(QWidget *parent)
    : QWidget(parent), m_cameraCalibWidget(nullptr), m_mirrorCalibWidget(nullptr), m_tabWidget(nullptr)
{
    init();
    this->setWindowTitle(QStringLiteral("")); 
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    this->sizePolicy().setHorizontalStretch(0);
    this->sizePolicy().setVerticalStretch(0);
    this->resize(1200, 800);
}

CalibWidget::~CalibWidget()
{
    delete m_cameraCalibWidget;
    delete m_mirrorCalibWidget;
    delete m_tabWidget;
}

void CalibWidget::init()
{
    m_cameraCalibWidget = new CameraCalibWidget(this);
    m_mirrorCalibWidget = new MirrorCalibWidget(this);
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(m_cameraCalibWidget, "相机标定");
    m_tabWidget->addTab(m_mirrorCalibWidget, "振镜校正");
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
     // 创建自定义标题栏
    CustomTitleBar* titleBar = new CustomTitleBar(this);
    titleBar->setTitle("标定");
    connect(titleBar, &CustomTitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(titleBar, &CustomTitleBar::closeClicked, this, &QWidget::close);
    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(m_tabWidget);
    mainLayout->setSpacing(0);
}