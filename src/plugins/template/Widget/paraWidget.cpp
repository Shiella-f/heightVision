#include "paraWidget.h"
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
#include <QMessageBox> 
#include <QDebug> 
#include <QTimer>
#include <QGroupBox>
#include <QFileDialog>
#include <QSlider>



ParaWidget::ParaWidget(QWidget* parent)
    : QWidget(parent)
{
    init();
}

ParaWidget::~ParaWidget() = default; 

void ParaWidget::init()
{
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

    QLabel* LearnFeaturelLabel = newLabel(new QLabel, QStringLiteral("最小特征点数量:"));
    m_learnFeatureSpinBox = new QSpinBox(this);
    m_learnFeatureSpinBox->setRange(128, 500);
    m_learnFeatureSpinBox->setValue(128);
    m_learnFeatureSpinBox->setSingleStep(5);
    QHBoxLayout* FeatureNumLayout = newHorizontalLayout(new QHBoxLayout);
    FeatureNumLayout->addWidget(LearnFeaturelLabel);
    FeatureNumLayout->addWidget(m_learnFeatureSpinBox);

    QLabel* MatchAngleLabel = newLabel(new QLabel, QStringLiteral("匹配角度:"));//
    m_matchAngleSpinBox = new QDoubleSpinBox(this);
    m_matchAngleSpinBox->setRange(0.0, 360.0);
    m_matchAngleSpinBox->setValue(360.0);
    m_matchAngleSpinBox->setSingleStep(0.5);
    QHBoxLayout* angleLayout = newHorizontalLayout(new QHBoxLayout);
    angleLayout->addWidget(MatchAngleLabel);
    angleLayout->addWidget(m_matchAngleSpinBox);

    QLabel* AngleStepLabel = newLabel(new QLabel, QStringLiteral("角度步长:"));//
    m_angleStepSpinBox = new QDoubleSpinBox(this);
    m_angleStepSpinBox->setRange(0.1, 360.0);
    m_angleStepSpinBox->setValue(1.0);
    m_angleStepSpinBox->setSingleStep(0.1);
    QHBoxLayout* angleStepLayout = newHorizontalLayout(new QHBoxLayout);
    angleStepLayout->addWidget(AngleStepLabel);
    angleStepLayout->addWidget(m_angleStepSpinBox);

    QLabel* MinScaleLabel = newLabel(new QLabel, QStringLiteral("最小缩放比例:"));//
    m_minScaleSpinBox = new QDoubleSpinBox(this);
    m_minScaleSpinBox->setRange(0.5, 1.0);
    m_minScaleSpinBox->setValue(0.95);
    QHBoxLayout* minScaleLayout = newHorizontalLayout(new QHBoxLayout);
    minScaleLayout->addWidget(MinScaleLabel);
    minScaleLayout->addWidget(m_minScaleSpinBox);

    QLabel* MaxScaleLabel = newLabel(new QLabel, QStringLiteral("最大缩放比例:"));//
    m_maxScaleSpinBox = new QDoubleSpinBox(this);
    m_maxScaleSpinBox->setRange(1.0, 1.5);
    m_maxScaleSpinBox->setValue(1.05);
    QHBoxLayout* maxScaleLayout = newHorizontalLayout(new QHBoxLayout);
    maxScaleLayout->addWidget(MaxScaleLabel);
    maxScaleLayout->addWidget(m_maxScaleSpinBox);

    QLabel* ScaleStepLabel = newLabel(new QLabel, QStringLiteral("缩放步长:"));//
    m_scaleStepSpinBox = new QDoubleSpinBox(this);
    m_scaleStepSpinBox->setRange(0.01, 1.0);
    m_scaleStepSpinBox->setValue(0.01);
    QHBoxLayout* scaleStepLayout = newHorizontalLayout(new QHBoxLayout);
    scaleStepLayout->addWidget(ScaleStepLabel);
    scaleStepLayout->addWidget(m_scaleStepSpinBox);

    m_polarityCheckBox = new QCheckBox(QStringLiteral("忽略极性"), this);
    m_polarityCheckBox->setChecked(true);
    m_subPixelCheckBox = new QCheckBox(QStringLiteral("亚像素"), this);
    m_subPixelCheckBox->setChecked(true);
    QHBoxLayout* checkBoxLayout = newHorizontalLayout(new QHBoxLayout);
    checkBoxLayout->addWidget(m_polarityCheckBox);
    checkBoxLayout->addWidget(m_subPixelCheckBox);


    auto newDoubleSlider = [](QWidget *p, int p_min, int p_max, int step, int value, QString qss = QString())
            {
                auto pSlider = new QSlider(Qt::Horizontal, p);
                pSlider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
                pSlider->setValue(value);
                pSlider->setMinimum(p_min);   
                pSlider->setMaximum(p_max); 
                pSlider->setSingleStep(step);
                if (!qss.isEmpty()) {
                    pSlider->setStyleSheet(qss);
                }
                return pSlider;
            };
    
    m_weakThresholdSpinBox = new QSpinBox(this);
    m_weakThresholdSpinBox->setRange(5, 160);
    m_weakThresholdSpinBox->setValue(60);

    QLabel* weakThresholdLabel = newLabel(new QLabel, QStringLiteral("模板对比度:"));
    QHBoxLayout* m_weakThresholdLayout = newHorizontalLayout(new QHBoxLayout);
    m_weakThresholdLayout->setAlignment(Qt::AlignLeft);
    m_weakThresholdLayout->addWidget(weakThresholdLabel); 

    m_weakThresholdLayout->addWidget(m_weakThresholdSpinBox);

    m_strongThresholdSpinBox = new QSpinBox(this);
    m_strongThresholdSpinBox->setRange(1, 160);
    m_strongThresholdSpinBox->setValue(30);

    QLabel* strongThresholdLabel = newLabel(new QLabel, QStringLiteral("匹配对比度:"));
    QHBoxLayout* m_strongThresholdLayout = newHorizontalLayout(new QHBoxLayout);
    m_strongThresholdLayout->setAlignment(Qt::AlignLeft);
    m_strongThresholdLayout->addWidget(strongThresholdLabel);
    m_strongThresholdLayout->addWidget(m_strongThresholdSpinBox);

    QVBoxLayout* CreatParasLayout = new QVBoxLayout; // 右侧参数纵向布局
    CreatParasLayout->addLayout(angleLayout);
    CreatParasLayout->addLayout(angleStepLayout);
    CreatParasLayout->addLayout(minScaleLayout);
    CreatParasLayout->addLayout(maxScaleLayout);
    CreatParasLayout->addLayout(scaleStepLayout);
    CreatParasLayout->addLayout(FeatureNumLayout);
    CreatParasLayout->addLayout(checkBoxLayout);
    CreatParasLayout->addLayout(m_weakThresholdLayout);
    CreatParasLayout->addLayout(m_strongThresholdLayout);

    QGroupBox* createGroupBox = new QGroupBox(QStringLiteral("模板创建参数"));
    createGroupBox->setLayout(CreatParasLayout);

    QLabel* MaxNumberLabel = newLabel(new QLabel, QStringLiteral("最大数量:")); // 最大数量标签
    m_maxNumberSpinBox = new QSpinBox(this);
    m_maxNumberSpinBox->setRange(1, 100);
    m_maxNumberSpinBox->setValue(10);
    QHBoxLayout* numberLayout = newHorizontalLayout(new QHBoxLayout);
    numberLayout->addWidget(MaxNumberLabel);
    numberLayout->addWidget(m_maxNumberSpinBox);

    QLabel* MatchCenterLabel = newLabel(new QLabel, QStringLiteral("匹配中心:"));
    m_matchCenterComboBox = new QComboBox(this);
    m_matchCenterComboBox->addItem(QStringLiteral("几何中心"));
    m_matchCenterComboBox->addItem(QStringLiteral("训练中心"));
    QHBoxLayout* centerLayout = newHorizontalLayout(new QHBoxLayout);
    centerLayout->addWidget(MatchCenterLabel);
    centerLayout->addWidget(m_matchCenterComboBox);

    QLabel* MatchScoreLabel = newLabel(new QLabel, QStringLiteral("匹配分数:"));
    m_matchScoreSpinBox = new QDoubleSpinBox(this);
    m_matchScoreSpinBox->setRange(1.0, 100.0);
    m_matchScoreSpinBox->setValue(70.0);
    QHBoxLayout* scoreLayout = newHorizontalLayout(new QHBoxLayout);
    scoreLayout->addWidget(MatchScoreLabel);
    scoreLayout->addWidget(m_matchScoreSpinBox);

    QVBoxLayout* FindParasLayout = new QVBoxLayout; // 右侧参数纵向布局
    FindParasLayout->addLayout(numberLayout);
    FindParasLayout->addLayout(centerLayout);
    FindParasLayout->addLayout(scoreLayout);

    QGroupBox* findGroupBox = new QGroupBox(QStringLiteral("匹配参数"));
    findGroupBox->setLayout(FindParasLayout);

    QVBoxLayout* rightLayout = new QVBoxLayout; // 右侧整体纵向布局
    rightLayout->addWidget(createGroupBox);
    rightLayout->addWidget(findGroupBox);
    this->setLayout(rightLayout);
}

MatchParams ParaWidget::buildMatchParams()
{
    MatchParams params;
    params.angleRange = m_matchAngleSpinBox ? m_matchAngleSpinBox->value() : 360.0;
    params.angle_step = m_angleStepSpinBox ? m_angleStepSpinBox->value() : 0.5;
    params.FeaturePointNum = m_learnFeatureSpinBox->value();
    params.weakThreshold = m_weakThresholdSpinBox->value();
    params.strongThreshold = m_strongThresholdSpinBox->value();
    params.ignorePolarity = m_polarityCheckBox && m_polarityCheckBox->isChecked();
    params.scale_min = m_minScaleSpinBox ? m_minScaleSpinBox->value() : 0.9f;
    params.scale_max = m_maxScaleSpinBox ? m_maxScaleSpinBox->value() : 1.1f;
    params.scale_step = m_scaleStepSpinBox ? m_scaleStepSpinBox->value() : 0.02f;
    return params;
}

FindMatchParams ParaWidget::buildFindMatchParams(bool useRoi)
{
    FindMatchParams params;
    params.maxCount = m_maxNumberSpinBox ? m_maxNumberSpinBox->value() : 1;
    params.useSubPx = m_subPixelCheckBox && m_subPixelCheckBox->isChecked();
    params.scoreThreshold =  m_matchScoreSpinBox ? m_matchScoreSpinBox->value() : 70.0;
    const bool useTrainCenter = m_matchCenterComboBox && m_matchCenterComboBox->currentIndex() == 0;
    params.centerType = useTrainCenter ? MatchCenterType::SceneCenter : MatchCenterType::TrainCenter;
    params.useRoi = useRoi;
    return params;
}