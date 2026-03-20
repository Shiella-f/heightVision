#include "camerapara.h"
#include <QDebug>
#include <QBoxLayout>

CameraPara::CameraPara(QWidget* parent) : QWidget(parent) {
    this->hide();
    init();
}

CameraPara::~CameraPara() {
}

// Helper to emit signal whenever parameters change
void CameraPara::updateParameters() {
    emit parametersChanged(m_camerapara);
}

void CameraPara::init() {
    // 初始化摄像头参数
    m_camerapara.autoExposureSupported = true;
    m_camerapara.exposureValue = 0;
    m_camerapara.gainValue = 64;
    m_camerapara.brightnessValue = 0;
    m_camerapara.contrastValue = 38;
    m_camerapara.saturationValue = 64;
    m_camerapara.toneValue = 0;
    m_camerapara.gammaValue = 300;
    m_camerapara.fpsValue = 15;
    m_camerapara.autoFocusSupported = true;
    m_camerapara.focusValue = 800;

    m_autoExposureCheckBox = new QCheckBox(QStringLiteral("自动曝光"), this);
    m_autoExposureCheckBox->setChecked(true);
    connect(m_autoExposureCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_exposureSlider->setEnabled(!checked);
        m_camerapara.autoExposureSupported = checked;
        updateParameters();
    });
    
    auto newslider = [this](QSlider* s, const QString& name, int min=0, int max=100, int value=50) {
        s = new QSlider(this);
        s->setOrientation(Qt::Horizontal);
        s->setRange(min, max);
        s->setValue(value);
        s->setToolTip(name);
        return s;
    };
    
    m_exposureSlider = newslider(new QSlider(this), QStringLiteral("曝光"), -14, 0, 0);
    connect(m_exposureSlider, &QSlider::valueChanged, this, [this](int value) {
        m_camerapara.exposureValue = value;
        updateParameters();
    });
    
    m_gainSlider = newslider(new QSlider(this), QStringLiteral("增益"), 0, 128, 64);
    connect(m_gainSlider, &QSlider::valueChanged, this, [this](int value) {
        m_camerapara.gainValue = value;
        updateParameters();
    });
    
    m_brightnessSlider = newslider(new QSlider(this), QStringLiteral("亮度"), -64, 64, 0);
    connect(m_brightnessSlider, &QSlider::valueChanged, this, [this](int value) {
        m_camerapara.brightnessValue = value;
        updateParameters();
    });
    
    m_contrastSlider = newslider(new QSlider(this), QStringLiteral("对比度"), 0, 100, 38);
    connect(m_contrastSlider, &QSlider::valueChanged, this, [this](int value) {
        m_camerapara.contrastValue = value;
        updateParameters();
    });
    
    m_saturationSlider = newslider(new QSlider(this), QStringLiteral("饱和度"), 0, 100, 64);
    connect(m_saturationSlider, &QSlider::valueChanged, this, [this](int value) {
        m_camerapara.saturationValue = value;
        updateParameters();
    });
    
    m_toneSlider = newslider(new QSlider(this), QStringLiteral("色调"), -180, 180, 0);
    connect(m_toneSlider, &QSlider::valueChanged, this, [this](int value) {
        m_camerapara.toneValue = value;
        updateParameters();
    });
    
    m_gammaSlider = newslider(new QSlider(this), QStringLiteral("伽马值"), 100, 500, 300);
    connect(m_gammaSlider, &QSlider::valueChanged, this, [this](int value) {
        m_camerapara.gammaValue = value;
        updateParameters();
    });
    
    m_autoFocusCheckBox = new QCheckBox(QStringLiteral("自动对焦"), this);
    m_autoFocusCheckBox->setChecked(true);
    connect(m_autoFocusCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_focusSlider->setEnabled(!checked);
        m_camerapara.autoFocusSupported = checked;
        updateParameters();
    });
    
    m_focusSlider = newslider(new QSlider(this), QStringLiteral("对焦"), 0, 1000, 800);
    connect(m_focusSlider, &QSlider::valueChanged, this, [this](int value) {
        m_camerapara.focusValue = value;
        updateParameters();
    });

    m_fpsSlider = newslider(new QSlider(this), QStringLiteral("帧率"), 1, 60, 15);
    connect(m_fpsSlider, &QSlider::valueChanged, this, [this](int value) {
        m_camerapara.fpsValue = value;
        updateParameters();
    });

    QBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(m_autoExposureCheckBox);
    layout->addWidget(m_exposureSlider);
    layout->addWidget(m_gainSlider);
    layout->addWidget(m_brightnessSlider);
    layout->addWidget(m_contrastSlider);
    layout->addWidget(m_saturationSlider);
    layout->addWidget(m_toneSlider);
    layout->addWidget(m_gammaSlider);
    layout->addWidget(m_autoFocusCheckBox);
    layout->addWidget(m_focusSlider);
    layout->addWidget(m_fpsSlider);
}
