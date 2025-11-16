#include "matchWidget.h"
#include <QWidget>
#include <QToolButton>
#include <QSizePolicy>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QGraphicsView>

#include "Scene/zoomScene.h"
#include "Scene/ImageDisplayScene.h"

    const QString buttonStyle = QStringLiteral(
            "QToolButton{border-radius: 5px;solid gray;color:black;text-align:center;}"
            "QToolButton:hover{border:1px solid black;background-color:rgb(200,200,200);color:black;}"
            "QToolButton:pressed{background-color:rgb(150,150,150);color:white;}"
        );

matchWidget::matchWidget(QWidget* parent)
    : QWidget(parent),
      m_heightMainWindow(nullptr)
{
    m_currentImage = cv::imread("E:\\lipoDemo\\HeiVersion\\res\\image\\Matchres\\9.png");
    init();
    this->setWindowTitle(QStringLiteral("Height Measurement Tool"));
    this->setFixedSize(800, 600);
    displayImage(m_currentImage);

}

matchWidget::~matchWidget() = default;

void matchWidget::init() 
{

    auto newButton = [](QToolButton* btn, const QString& text) {
        btn->setText(text);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        btn->setStyleSheet(buttonStyle);
        btn->setFixedSize(80, 20);
        return btn;
    };
    auto newLabel = [](QLabel* label, const QString& text) {
        label->setText(text);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return label;
    };

    auto newHorizontalLayout = [](QHBoxLayout* layout) {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        return layout;
    };

    QLabel* MaxNumberLabel = newLabel(new QLabel, QStringLiteral("最大数量:"));
    QSpinBox* MaxNumberSpinBox = new QSpinBox;
    MaxNumberSpinBox->setRange(1, 100);
    MaxNumberSpinBox->setValue(10);
    QHBoxLayout* numberLayout = newHorizontalLayout(new QHBoxLayout);
    numberLayout->addWidget(MaxNumberLabel);
    numberLayout->addWidget(MaxNumberSpinBox);

    QLabel* MatchAngleLabel = newLabel(new QLabel, QStringLiteral("匹配角度:"));
    QDoubleSpinBox* MatchAngleSpinBox = new QDoubleSpinBox;
    MatchAngleSpinBox->setRange(0.0, 360.0);
    MatchAngleSpinBox->setValue(180.0);
    QHBoxLayout* angleLayout = newHorizontalLayout(new QHBoxLayout);
    angleLayout->addWidget(MatchAngleLabel);
    angleLayout->addWidget(MatchAngleSpinBox);

    QLabel* MatchCenterLabel = newLabel(new QLabel, QStringLiteral("匹配中心:"));
    QComboBox* MatchCenterComboBox = new QComboBox;
    MatchCenterComboBox->addItem(QStringLiteral("框选中心"));
    MatchCenterComboBox->addItem(QStringLiteral("图像中心"));
    QHBoxLayout* centerLayout = newHorizontalLayout(new QHBoxLayout);
    centerLayout->addWidget(MatchCenterLabel);
    centerLayout->addWidget(MatchCenterComboBox);

    QLabel* MatchScoreLabel = newLabel(new QLabel, QStringLiteral("匹配分数:"));
    QDoubleSpinBox* MatchScoreSpinBox = new QDoubleSpinBox;
    MatchScoreSpinBox->setRange(1.0, 100.0);
    MatchScoreSpinBox->setValue(70.0);
    QHBoxLayout* scoreLayout = newHorizontalLayout(new QHBoxLayout);
    scoreLayout->addWidget(MatchScoreLabel);
    scoreLayout->addWidget(MatchScoreSpinBox);

    QLabel* LearnDetailLabel = newLabel(new QLabel, QStringLiteral("学习细节:"));
    QSpinBox* LearnDetailSpinBox = new QSpinBox;
    LearnDetailSpinBox->setRange(1, 10);
    LearnDetailSpinBox->setValue(5);
    QHBoxLayout* detailLayout = newHorizontalLayout(new QHBoxLayout);
    detailLayout->addWidget(LearnDetailLabel);
    detailLayout->addWidget(LearnDetailSpinBox);

    QCheckBox* PolarityCheckBox = new QCheckBox(QStringLiteral("忽略极性"));
    PolarityCheckBox->setChecked(true);
    QCheckBox* ScoreCheckBox = new QCheckBox(QStringLiteral("严格评分"));
    ScoreCheckBox->setChecked(true);
    QHBoxLayout* checkBoxLayout = newHorizontalLayout(new QHBoxLayout);
    checkBoxLayout->addWidget(PolarityCheckBox);
    checkBoxLayout->addWidget(ScoreCheckBox);

    m_setRoiBtn = newButton(new QToolButton, QStringLiteral("框选ROI"));
    connect(m_setRoiBtn, &QToolButton::clicked, [this]() {
        m_ImageDisplayScene->setRoiSelectionEnabled(true);
    });
    m_confirmMatchBtn = newButton(new QToolButton, QStringLiteral("全图匹配"));
    connect(m_confirmMatchBtn, &QToolButton::clicked, this, &matchWidget::confirmMatch);
    m_learnMatchBtn = newButton(new QToolButton, QStringLiteral("学习"));
    connect(m_learnMatchBtn, &QToolButton::clicked, this, &matchWidget::learnMatch);
    m_confirmRoiMatchBtn = newButton(new QToolButton, QStringLiteral("ROI内匹配"));
    connect(m_confirmRoiMatchBtn, &QToolButton::clicked, this, &matchWidget::confirmRoiMatch);
    m_testHeightBtn = newButton(new QToolButton(this), QStringLiteral("高度测量"));
    connect(m_testHeightBtn, &QToolButton::clicked, this, &matchWidget::TestHeight);
    m_confirmRoiareaBtn = newButton(new QToolButton, QStringLiteral("确认ROI区域"));
    connect(m_confirmRoiareaBtn, &QToolButton::clicked, [this]() {
        QRectF roi;
        if (!m_ImageDisplayScene->getRoiArea(roi)) {
            m_RoiDisplayScene->showPlaceholder(QStringLiteral("No ROI"));
            return;
        }
        m_Roi = QRectF(roi.left(), roi.top(), roi.width(), roi.height());
        displayRoiImage(m_Roi);
    });
    QGridLayout* btnLayout = new QGridLayout;
    btnLayout->addWidget(m_setRoiBtn, 0, 0);
    btnLayout->addWidget(m_learnMatchBtn, 0, 1);
    btnLayout->addWidget(m_confirmMatchBtn, 1, 0);
    btnLayout->addWidget(m_confirmRoiMatchBtn, 1, 1);
    btnLayout->addWidget(m_confirmRoiareaBtn, 2, 0);
    btnLayout->addWidget(m_testHeightBtn, 2, 1);

    QLabel* PtsLabel = new QLabel(QStringLiteral("点击位置图像坐标 "), this);
    QLabel* PtsValueLabel = new QLabel(QStringLiteral("  ,  "), this);
    QHBoxLayout* PtsLayout = newHorizontalLayout(new QHBoxLayout);
    PtsLayout->addWidget(PtsLabel);
    PtsLayout->addWidget(PtsValueLabel);

    QGraphicsView* roiImageView = new QGraphicsView(this);
    m_RoiDisplayScene = new ImageDisplayScene(roiImageView);
    roiImageView->setScene(m_RoiDisplayScene);
    roiImageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    roiImageView->setFixedSize(160, 160);
    roiImageView->setStyleSheet("background-color: rgb(40, 40, 40);border:1px solid gray;");
    QVBoxLayout* ParasLayout = new QVBoxLayout;
    ParasLayout->addWidget(roiImageView);
    ParasLayout->addLayout(numberLayout);
    ParasLayout->addLayout(centerLayout);
    ParasLayout->addLayout(scoreLayout);
    ParasLayout->addLayout(detailLayout);
    ParasLayout->addLayout(checkBoxLayout);
    ParasLayout->addLayout(btnLayout);
    ParasLayout->addLayout(PtsLayout);

    ParasLayout->addStretch();
    ParasLayout->setStretch(0, 13);
    ParasLayout->setStretch(1, 1);
    ParasLayout->setStretch(2, 1);
    ParasLayout->setStretch(3, 1);
    ParasLayout->setStretch(4, 1);
    ParasLayout->setStretch(5, 1);
    ParasLayout->setStretch(6, 1);
    ParasLayout->setStretch(7, 1);
    ParasLayout->setStretch(8, 1);
    ParasLayout->setStretch(9, 1);
    QWidget* paramsWidget = new QWidget;
    paramsWidget->setLayout(ParasLayout);

    m_ImageDisplayScene = new ZoomScene(this);
    QGraphicsView* imageView = new QGraphicsView(this);
    m_ImageDisplayScene->attachView(imageView);
    imageView->setStyleSheet("background-color: rgb(40, 40, 40);border:1px solid gray;");

    QGridLayout* mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->addWidget(imageView, 0, 0, 10, 8);
    mainLayout->addWidget(paramsWidget, 0, 8, 2, 2);
    this->setLayout(mainLayout);

}

void matchWidget::setRoi()
{

}
void matchWidget::learnMatch()
{

}
void matchWidget::confirmMatch()
{
    
}
void matchWidget::confirmRoiMatch()
{

}

void matchWidget::confirmRoiarea()
{

}

void matchWidget::TestHeight()
{
    m_heightMainWindow = new HeightMainWindow(nullptr);
    m_heightMainWindow->show();
}

void matchWidget::displayImage(const cv::Mat &image)
{
    if (!m_ImageDisplayScene) {
        return;
    }

    if (image.empty()) {
        m_ImageDisplayScene->clearImage();
        m_ImageDisplayScene->showPlaceholder(QStringLiteral("No Image"));
        return;
    }

    QImage qImage;
    if (!bscvTool::cvImg2QImage(image, qImage)) {
        qWarning() << "Failed to convert cv::Mat to QImage";
        m_ImageDisplayScene->showPlaceholder(QStringLiteral("Convert Failed"));
        return;
    }

    m_ImageDisplayScene->setOriginalPixmap(QPixmap::fromImage(qImage));
    m_ImageDisplayScene->setSourceImageSize(QSize(image.cols, image.rows));
}

void matchWidget::displayRoiImage(const QRectF &roi)
{
    if (!m_RoiDisplayScene) {
        return;
    }

    if (!m_ImageDisplayScene->hasImage()) {
        m_RoiDisplayScene->clearImage();
        m_RoiDisplayScene->showPlaceholder(QStringLiteral("No Image"));
        return;
    }
    QImage sourceImage = m_ImageDisplayScene->getSourceImageMat();

    if (sourceImage.isNull()) {
        m_RoiDisplayScene->clearImage();
        m_RoiDisplayScene->showPlaceholder(QStringLiteral("No Image"));
        return;
    }

    QImage roiImage = sourceImage.copy(static_cast<int>(roi.x()), static_cast<int>(roi.y()),
        static_cast<int>(roi.width()), static_cast<int>(roi.height()));


    m_RoiDisplayScene->setOriginalPixmap(QPixmap::fromImage(roiImage));
    m_RoiDisplayScene->setSourceImageSize(QSize(roiImage.width(), roiImage.height()));
}
