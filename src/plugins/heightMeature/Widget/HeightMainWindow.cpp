#include "HeightMainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>
#include <QDebug>
#include <QInputDialog>
#include <vector>
#include <QToolButton>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QGraphicsView>
#include <QPixmap>
#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>

#include "Scene/HeightScene.h"

namespace {
void showMessage(QWidget* parent, const QString& title, const QString& text, QMessageBox::Icon icon = QMessageBox::Information) {
    QMessageBox box(icon, title, text, QMessageBox::Ok, parent);
    box.exec();
}
} // namespace

const QString buttonStyle = QStringLiteral(
    "QToolButton{border-radius: 5px;solid gray;color:black;text-align:center;}"
    "QToolButton:hover{border:1px solid black;background-color:rgb(200,200,200);color:black;}"
    "QToolButton:pressed{background-color:rgb(150,150,150);color:white;}"
    "QToolButton:disabled{background-color:rgb(40,40,40);color:white;}"
);

HeightMainWindow::HeightMainWindow(QWidget* parent)
    : QWidget(parent),m_CameraImgShowTimer(nullptr),
      m_heightCore(std::make_unique<Height::core::HeightCore>())
{
    init();
    this->setWindowTitle(QStringLiteral("Height Measurement Tool"));
    this->setFixedSize(800, 600);
}

HeightMainWindow::~HeightMainWindow() = default;

bool HeightMainWindow::isCalibrated() const
{
    return m_heightCore && m_heightCore->hasLinearCalib();
}

void HeightMainWindow::init() 
{
    auto newButton = [](QToolButton* btn, const QString& text) {
        btn->setText(text);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        btn->setStyleSheet(buttonStyle);
        btn->setFixedSize(100, 20);
        return btn;
    };
    m_CameraImgShowTimer = new QTimer(this);
    connect(m_CameraImgShowTimer, &QTimer::timeout, this, [this]() {
        if (!CameraImg.empty()) {
            displayImage(CameraImg);
            m_heightCore->loadTestImageInfo(CameraImg);
        }
    });
    m_loadFileBtn = newButton(new QToolButton(this), QStringLiteral("加载文件夹 "));
    m_SetPreferenceBtn = newButton(new QToolButton(this), QStringLiteral("设置基准 "));
    m_SetPreferenceBtn->setEnabled(false);
    m_selectImageBtn = newButton(new QToolButton(this), QStringLiteral("选择图片 "));
    m_startTestBtn = newButton(new QToolButton(this), QStringLiteral("开始测高 "));
    m_selectROIBtn = newButton(new QToolButton(this), QStringLiteral("设置ROI "));
    m_confirmROIBtn = newButton(new QToolButton(this), QStringLiteral("确认ROI "));
    m_saveCalibrationDataBtn = newButton(new QToolButton(this), QStringLiteral("保存标定数据 "));
    m_loadCalibrationDataBtn = newButton(new QToolButton(this), QStringLiteral("加载标定数据 "));
    m_OpenCameraBtn = newButton(new QToolButton(this), QStringLiteral("显示当前照片 "));
    m_OpenCameraBtn->setCheckable(true);
    m_OpenCameraBtn->setText("显示当前照片");
    connect(m_OpenCameraBtn, &QToolButton::toggled, this, &HeightMainWindow::OpenCameraBtnToggled);

    connect(m_loadFileBtn, &QToolButton::clicked, this, &HeightMainWindow::LoadFile);
    connect(m_startTestBtn, &QToolButton::clicked, this, &HeightMainWindow::StartTest);
    connect(m_selectImageBtn, &QToolButton::clicked, this, &HeightMainWindow::SelectImage);
    connect(m_SetPreferenceBtn, &QToolButton::clicked, this, &HeightMainWindow::SetPreference);
    connect(m_saveCalibrationDataBtn, &QToolButton::clicked, this, &HeightMainWindow::onCalibrationDataSaved);
    connect(m_loadCalibrationDataBtn, &QToolButton::clicked, this, &HeightMainWindow::onCalibrationDataLoaded);
    connect(m_selectROIBtn, &QToolButton::clicked, [this]() {
        if(m_zoomScene) m_zoomScene->setRoiSelectionEnabled(true);
    });
    connect(m_confirmROIBtn, &QToolButton::clicked, [this]() {
        if(m_zoomScene) {
            QRectF roi;
            m_SetPreferenceBtn->setEnabled(true);
            m_zoomScene->getRoiArea(roi);
            GetROI();
        }
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

    m_isDisplayprocessImg = new QCheckBox(QStringLiteral("显示处理图像"), this);
    m_isDisplayprocessImg->setChecked(false);
    connect(m_isDisplayprocessImg, &QCheckBox::stateChanged, this, [this](int state){
        if(m_heightCore) {
            m_heightCore->setProcessedIsDisplay(state == Qt::Checked);
        }
    });
    QLabel* displayLabel = new QLabel(QStringLiteral("  光斑识别阈值:"), this);
    displayLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_thresholdBox = new QSpinBox(m_testAreaGroupBox);
    m_thresholdBox->setRange(0, 255);
    m_thresholdBox->setValue(180);
    m_thresholdBox->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(m_thresholdBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value){
        if(m_heightCore) {
            m_heightCore->setThreshold(value);
        }
    });

    QGridLayout* QGroupBoxLayout = new QGridLayout(m_testAreaGroupBox);
    QGroupBoxLayout->setContentsMargins(5, 5, 5, 5);
    QGroupBoxLayout->setSpacing(5);
    QGroupBoxLayout->addWidget(m_loadFileBtn, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    QGroupBoxLayout->addWidget(m_SetPreferenceBtn, 0, 1);
    QGroupBoxLayout->addWidget(m_selectImageBtn, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
    QGroupBoxLayout->addWidget(m_startTestBtn, 1, 1);
    QGroupBoxLayout->addWidget(m_selectROIBtn, 2, 0, Qt::AlignLeft | Qt::AlignVCenter);
    QGroupBoxLayout->addWidget(m_confirmROIBtn, 2, 1);
    QGroupBoxLayout->addWidget(m_saveCalibrationDataBtn, 3, 0, Qt::AlignLeft | Qt::AlignVCenter);
    QGroupBoxLayout->addWidget(m_loadCalibrationDataBtn, 3, 1);
    QGroupBoxLayout->addWidget(m_OpenCameraBtn, 4, 0, Qt::AlignLeft | Qt::AlignVCenter);
    QGroupBoxLayout->addWidget(displayLabel, 5, 0, Qt::AlignLeft | Qt::AlignVCenter);
    QGroupBoxLayout->addWidget(m_thresholdBox, 5, 1);
    //QGroupBoxLayout->addWidget(m_isDisplayprocessImg, 6, 0);

    QGroupBoxLayout->setRowStretch(6, 1);

    QGroupBoxLayout->addWidget(m_currentHeightLabel, 7, 0, Qt::AlignRight | Qt::AlignVCenter);
    QGroupBoxLayout->addWidget(m_resultLabel, 7, 1, Qt::AlignLeft | Qt::AlignVCenter);
    m_testAreaGroupBox->setLayout(QGroupBoxLayout);

    m_zoomScene = new HeightScene(this);
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

void HeightMainWindow::displayImage(const cv::Mat &image)
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
        if(image.type() == CV_8UC3) {
        const uchar *pSrc = (const uchar *)image.data;
        qImage = QImage(pSrc, image.cols, image.rows, image.step, QImage::Format_RGB888).rgbSwapped();
        }

    if (qImage.isNull()) {
        qWarning() << "Failed to convert cv::Mat to QImage";
        m_zoomScene->showPlaceholder(QStringLiteral("Convert Failed"));
        return;
    }

    m_zoomScene->setOriginalPixmap(QPixmap::fromImage(qImage));
    m_zoomScene->setSourceImageSize(QSize(image.cols, image.rows));
    QTimer::singleShot(0, this, [this](){ if (m_zoomScene) m_zoomScene->resetImage(); });
}

void HeightMainWindow::setHeightdisplay(const double& height)
{
    if(m_resultLabel)
        m_resultLabel->setText(QString::number(height, 'f', 4));
}

void HeightMainWindow::LoadFile() 
{
    if (!m_heightCore) {
        showMessage(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"), QMessageBox::Critical);
        return;
    }

    const QString folderPath = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择图片文件夹"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (folderPath.isEmpty()) {
        return;
    }

    const bool success = m_heightCore->loadFolder(folderPath);
    if (!success) {
        showMessage(this, QStringLiteral("提示"), QStringLiteral("未选择有效的图片文件夹"), QMessageBox::Information);
        return;
    }

    qDebug() << "Loaded image count:" << m_heightCore->getImageInfos().size();
    
    auto imageInfos = m_heightCore->getImageInfos();
    m_showImage = m_heightCore->getShowImage();
    displayImage(m_showImage);
}
//开始测高
void HeightMainWindow::StartTest() 
{
    if (!m_heightCore) {
        showMessage(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"), QMessageBox::Critical);
        return;
    }
    if(!m_heightCore->computeTestImageInfo()) {
        showMessage(this, QStringLiteral("提示"), QStringLiteral("图像光斑识别失败"), QMessageBox::Warning);
        return;
    }
    m_TestHeight = m_heightCore->measureHeightForImage().value_or(-1.0);
    if (m_TestHeight < 0.0) {
        showMessage(this, QStringLiteral("提示"), QStringLiteral("测高失败"), QMessageBox::Warning);
        return;
    }
    setHeightdisplay(m_TestHeight);
    showMessage(this, QStringLiteral("测高结果"), QStringLiteral("测量高度为: %1 mm").arg(m_TestHeight));
}
//选择图片显示测高
void HeightMainWindow::SelectImage() 
{
    if (!m_heightCore) {
        showMessage(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"), QMessageBox::Critical);
        return;
    }

    const QString m_testImagePath = QFileDialog::getOpenFileName(this, ("打开图片"), "", ("Images (*.png *.xpm *.jpg *.bmp);;All Files (*)"));
    if (m_testImagePath.isEmpty())
        return;

    if(!m_heightCore->loadTestImageInfo(m_testImagePath)) {
        showMessage(this, QStringLiteral("提示"), QStringLiteral("请选择有效的测试图片"), QMessageBox::Warning);
        return;
    }
    m_showImage = m_heightCore->getShowImage();
    displayImage(m_showImage);
    m_startTestBtn->setEnabled(true);
}
//设置参考高度
void HeightMainWindow::SetPreference() 
{
    if (!m_heightCore) {
        showMessage(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"), QMessageBox::Critical);
        return;
    }

    const QString preferenceImagePath = QFileDialog::getOpenFileName(this, ("打开图片"), "", ("Images (*.png *.xpm *.jpg *.bmp);;All Files (*)"));
    if (preferenceImagePath.isEmpty())
        return;

    if(!m_heightCore->importImageAsReference(preferenceImagePath)) {
        showMessage(this, QStringLiteral("提示"), QStringLiteral("请选择有效的参考图片(需在已加载列表中)"), QMessageBox::Warning);
        return;
    }

    bool prefHeight_ok;
    double prefHeight = QInputDialog::getDouble(
                nullptr,
                "输入参数",
                "请输入参考图片高度(mm):",
                20.0,                      // 默认值
                0.0,                   // 最小值
                999.0,                    // 最大值
                2,                        // 小数位数
                &prefHeight_ok
            );
        if (!prefHeight_ok) { // 用户取消输入
            return ;
        }

    if(!m_heightCore->setPreferenceHeight(prefHeight)) {
        showMessage(this, QStringLiteral("提示"), QStringLiteral("设置参考高度失败"), QMessageBox::Warning);
        return;
    }
    if(!m_heightCore->computeHeightValues()) {
        showMessage(this, QStringLiteral("提示"), QStringLiteral("参考高度值失败"), QMessageBox::Warning);
        return;
    }
    std::vector<double> distancePxList = m_heightCore->getDistancePxList();
    std::vector<double> heightList = m_heightCore->getHeightList();

    m_heightCore->setIsLinearCalib(m_heightCore->setCalibrationLinear());


    qDebug() << "Computed Heights:";
    for (size_t i = 0; i < heightList.size(); ++i) {
        qDebug() << "Image" << i << ": Distance Px =" << distancePxList[i]
                 << ", Height Mm =" << heightList[i];
    }

    auto imageInfos = m_heightCore->getImageInfos();
    m_heightCore->getCalibrationLinear(calibA, calibB);
    m_selectImageBtn->setEnabled(true);
}
//获取ROI
void HeightMainWindow::GetROI() 
{
    if (!m_heightCore || !m_zoomScene) {
        return;
    }
    QRectF roi;
    m_zoomScene->getRoiArea(roi);
    
    m_cvRoi = cv::Rect2f(static_cast<float>(roi.x()),
                         static_cast<float>(roi.y()),
                         static_cast<float>(roi.width()),
                         static_cast<float>(roi.height()));
    m_heightCore->setROI(m_cvRoi);
    qDebug() << "Selected ROI:"
             << "x =" << m_cvRoi.x
             << "y =" << m_cvRoi.y
             << "width =" << m_cvRoi.width
             << "height =" << m_cvRoi.height;
}

void HeightMainWindow::onCalibrationDataSaved()
{
    if (!m_heightCore) {
        showMessage(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"), QMessageBox::Critical);
        return;
    }

    const QString saveFilePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存标定数据"),
        "",
        QStringLiteral("Calibration Data (*.calib);;All Files (*)"));

    if (saveFilePath.isEmpty()) {
        return;
    }

    if (!m_heightCore->saveCalibrationData(saveFilePath)) {
        showMessage(this, QStringLiteral("错误"), QStringLiteral("保存标定数据失败"), QMessageBox::Critical);
        return;
    }

    showMessage(this, QStringLiteral("成功"), QStringLiteral("标定数据已保存"), QMessageBox::Information);
}

void HeightMainWindow::onCalibrationDataLoaded()
{
    if (!m_heightCore) {
        showMessage(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"), QMessageBox::Critical);
        return;
    }

    const QString loadFilePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("加载标定数据"),
        "",
        QStringLiteral("Calibration Data (*.calib);;All Files (*)"));

    if (loadFilePath.isEmpty()) {
        return;
    }

    if (!m_heightCore->loadCalibrationData(loadFilePath)) {
        showMessage(this, QStringLiteral("错误"), QStringLiteral("加载标定数据失败"), QMessageBox::Critical);
        return;
    }

    m_heightCore->setIsLinearCalib(true);
    m_heightCore->getCalibrationLinear(calibA, calibB);

    showMessage(this, QStringLiteral("成功"), QStringLiteral("标定数据已加载"), QMessageBox::Information);
}

void HeightMainWindow::OpenCameraBtnToggled()
{
    if (m_OpenCameraBtn->isChecked()) {
        m_OpenCameraBtn->setText("关闭当前照片");
        m_CameraImgShowTimer->start(66); // 每66ms更新一次图像显示
    } else {
        m_OpenCameraBtn->setText("显示当前照片");
        m_CameraImgShowTimer->stop();
    }
}

double HeightMainWindow::getMeasurementResult() const
{
    return m_TestHeight;
}
