#include "HeightMainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>
#include <QDebug>
#include <QInputDialog>


HeightMainWindow::HeightMainWindow(QWidget* parent)
    : QWidget(parent),
      m_testHeightWidget(nullptr),
      m_heightCore(std::make_unique<Height::core::HeightCore>())
{
    init();
    this->setWindowTitle(QStringLiteral("Height Measurement Tool"));

}

HeightMainWindow::~HeightMainWindow() = default;

void HeightMainWindow::init() 
{
    m_testHeightWidget = new TestHeightWidget(this);
    m_testHeightWidget->setFixedSize(800, 600);
    m_testHeightWidget->setStyleSheet("margin:5px;padding:5px;");
    m_testHeightWidget->setWindowTitle(QStringLiteral("Get Height"));
    m_testHeightWidget->show();

    connect(m_testHeightWidget, &TestHeightWidget::loadFileTriggered, this, &HeightMainWindow::LoadFile);
    connect(m_testHeightWidget, &TestHeightWidget::startTestTriggered, this, &HeightMainWindow::StartTest);
    connect(m_testHeightWidget, &TestHeightWidget::selectImageTriggered, this, &HeightMainWindow::SelectImage);
    connect(m_testHeightWidget, &TestHeightWidget::SetPreferenceTriggered, this, &HeightMainWindow::SetPreference);
    connect(m_testHeightWidget, &TestHeightWidget::confirmROITriggered, this, &HeightMainWindow::GetROI);
}

void HeightMainWindow::LoadFile() 
{
    if (!m_heightCore) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"));
        return;
    }

    const bool success = m_heightCore->loadFolder();
    if (!success) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("未选择有效的图片文件夹"));
        return;
    }

    qDebug() << "Loaded image count:" << m_heightCore->getImageInfos().size();

    //QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("图片文件夹加载完成"));
    
    auto imageInfos = m_heightCore->getImageInfos();
    m_showImage = m_heightCore->getShowImage();
    if (m_testHeightWidget) {
        m_testHeightWidget->displayImage(m_showImage);
    }
}

//开始测高
void HeightMainWindow::StartTest() 
{
    if (!m_heightCore) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"));
        return;
    }
    if(!m_heightCore->computeTestImageInfo()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("图像光斑识别失败"));
        return;
    }
    m_TestHeight = m_heightCore->measureHeightForImage().value_or(-1.0);
    if (m_TestHeight < 0.0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("测高失败"));
        return;
    }
    qDebug() << "Measured Height (mm):" << m_TestHeight;
    QMessageBox::information(this, QStringLiteral("测高结果"), QStringLiteral("测量高度为: %1 mm").arg(m_TestHeight));
}

//选择图片显示测高结果
void HeightMainWindow::SelectImage() 
{
    if (!m_heightCore) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"));
        return;
    }
    if(!m_heightCore->loadTestImageInfo()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择有效的测试图片"));
        return;
    }
    m_showImage = m_heightCore->getShowImage();
    if (m_testHeightWidget) {
        m_testHeightWidget->displayImage(m_showImage);
    }
}
//显示当前图片高度
void HeightMainWindow::SetPreference() 
{
        if (!m_heightCore) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"));
        return;
    }

    if(!m_heightCore->importImageAsReference()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择有效的参考图片"));
        return;
    }

    bool prefHeight_ok;
    double prefHeight = QInputDialog::getDouble(
                nullptr,
                "输入参数",
                "请输入参考图片高度(mm):",
                2.0,                      // 默认值
                0.0,                   // 最小值
                999.0,                    // 最大值
                2,                        // 小数位数
                &prefHeight_ok
            );
        if (!prefHeight_ok) { // 用户取消输入
            return ;
        }

    if(!m_heightCore->setPreferenceHeight(prefHeight)) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("设置参考高度失败"));
        return;
    }
    if(!m_heightCore->computeHeightValues()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("参考高度值失败"));
        return;
    }
    vector<double> distancePxList = m_heightCore->getDistancePxList();
    vector<double> heightList = m_heightCore->getHeightList();

    m_heightCore->setIsLinearCalib(m_heightCore->setCalibrationLinear());


    qDebug() << "Computed Heights:";
    for (size_t i = 0; i < heightList.size(); ++i) {
        qDebug() << "Image" << i << ": Distance Px =" << distancePxList[i]
                 << ", Height Mm =" << heightList[i];
    }

    auto imageInfos = m_heightCore->getImageInfos();

    m_heightCore->getCalibrationLinear(calibA, calibB);
    qDebug() << "拟合直线:y = " << calibA << "x + " << calibB;
}
//获取ROI
void HeightMainWindow::GetROI(const QRectF& roi) 
{
    if (!m_heightCore) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"));
        return;
    }
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
