#include "testHWidget.h"
#include <QToolButton>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QGraphicsView>
#include <QPixmap>
#include <QDebug>

#include "zoomScene/zoomScene.h"
#include "tools/bscvTool.h"

 const QString buttonStyle = QStringLiteral(
        "QToolButton{border-radius: 5px;solid gray;color:black;text-align:center;}"
        "QToolButton:hover{border:1px solid black;background-color:rgb(200,200,200);color:black;}"
        "QToolButton:pressed{background-color:rgb(150,150,150);color:white;}"
    );

TestHeightWidget::TestHeightWidget(QWidget *parent)
    : QWidget(parent)
{
    this->setWindowTitle(QStringLiteral("Get Height"));
    init();
}

TestHeightWidget::~TestHeightWidget() 
{

}

void TestHeightWidget::init() 
{
    auto newButton = [](QToolButton* btn, const QString& text) {
        btn->setText(text);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        btn->setStyleSheet(buttonStyle);
        btn->setFixedSize(100, 40);
        return btn;
    };

    m_loadFileBtn = newButton(new QToolButton(this), QStringLiteral("加载文件夹 "));
    m_SetPreferenceBtn = newButton(new QToolButton(this), QStringLiteral("设置基准 "));
    m_SetPreferenceBtn->setEnabled(false);
    m_selectImageBtn = newButton(new QToolButton(this), QStringLiteral("选择图片 "));
    m_startTestBtn = newButton(new QToolButton(this), QStringLiteral("开始测高 "));
    m_selectROIBtn = newButton(new QToolButton(this), QStringLiteral("设置ROI "));
    m_confirmROIBtn = newButton(new QToolButton(this), QStringLiteral("确认ROI "));

    connect(m_loadFileBtn, &QToolButton::clicked, this, &TestHeightWidget::loadFileTriggered);
    connect(m_startTestBtn, &QToolButton::clicked, this, &TestHeightWidget::startTestTriggered);
    connect(m_selectImageBtn, &QToolButton::clicked, this, &TestHeightWidget::selectImageTriggered);
    connect(m_SetPreferenceBtn, &QToolButton::clicked, this, &TestHeightWidget::SetPreferenceTriggered);
    connect(m_selectROIBtn, &QToolButton::clicked, [this]() {
        m_zoomScene->setRoiSelectionEnabled(true);
    });
    connect(m_confirmROIBtn, &QToolButton::clicked, [this]() {
        QRectF roi;
        m_SetPreferenceBtn->setEnabled(true);
        m_zoomScene->getRoiArea(roi);
        emit confirmROITriggered(roi);
    });

    m_currentHeightLabel = new QLabel(this);
    m_currentHeightLabel->setText(QStringLiteral("  当前高度  : "));
    m_currentHeightLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_resultLabel = new QLabel(this);
    m_resultLabel->setText(QString("0.0000"));
    m_resultLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_testAreaGroupBox = new QGroupBox(this);
    m_testAreaGroupBox->setTitle(QStringLiteral("测高"));
    m_testAreaGroupBox->setFixedSize(220, 200);

    QGridLayout* QGroupBoxLayout = new QGridLayout(m_testAreaGroupBox);
    QGroupBoxLayout->setContentsMargins(5, 5, 5, 5);
    QGroupBoxLayout->setSpacing(5);
    QGroupBoxLayout->addWidget(m_loadFileBtn, 0, 0);
    QGroupBoxLayout->addWidget(m_SetPreferenceBtn, 0, 1);
    QGroupBoxLayout->addWidget(m_selectImageBtn, 1, 0);
    QGroupBoxLayout->addWidget(m_startTestBtn, 1, 1);
    QGroupBoxLayout->addWidget(m_selectROIBtn, 2, 0);
    QGroupBoxLayout->addWidget(m_confirmROIBtn, 2, 1);
    QGroupBoxLayout->addWidget(m_currentHeightLabel, 3, 0);
    QGroupBoxLayout->addWidget(m_resultLabel, 3, 1);

    QGroupBoxLayout->setRowStretch(4, 1);
    m_testAreaGroupBox->setLayout(QGroupBoxLayout);

    m_zoomScene = new ZoomScene(this);
    QGraphicsView* Pre_img_view = new QGraphicsView(this);
    m_zoomScene->attachView(Pre_img_view);
    Pre_img_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    Pre_img_view->setMinimumSize(320, 240);
    Pre_img_view->setStyleSheet("QGraphicsView{border:1px solid black; background: #303030;}");
    Pre_img_view->setInteractive(true);
    m_zoomScene->showPlaceholder(QStringLiteral("Camera"));
    QGridLayout* mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(Pre_img_view, 0, 0, 6, 7);
    mainLayout->addWidget(m_testAreaGroupBox, 0, 7 ,1 ,2);
    this->setLayout(mainLayout);
}

void TestHeightWidget::displayImage(const cv::Mat &image)
{
    if (!m_zoomScene) {
        return;
    }

    if (image.empty()) {
        m_zoomScene->clearImage();
        m_zoomScene->showPlaceholder(QStringLiteral("No Image"));
        return;
    }

    QImage qImage;
    if (!bscvTool::cvImg2QImage(image, qImage)) {
        qWarning() << "Failed to convert cv::Mat to QImage";
        m_zoomScene->showPlaceholder(QStringLiteral("Convert Failed"));
        return;
    }

    m_zoomScene->setOriginalPixmap(QPixmap::fromImage(qImage));
    m_zoomScene->setSourceImageSize(QSize(image.cols, image.rows));
}