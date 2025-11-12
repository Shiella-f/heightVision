#include "MainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>
#include <QDebug>
#include <QInputDialog>


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_testHeightWidget(nullptr),
      m_heightCore(std::make_unique<Height::core::HeightCore>())
{
    init();

}

MainWindow::~MainWindow() = default;

void MainWindow::init() 
{
    m_testHeightWidget = new TestHeightWidget(nullptr);
    m_testHeightWidget->setFixedSize(800, 600);
    m_testHeightWidget->setStyleSheet("margin:5px;padding:5px;");
    m_testHeightWidget->setWindowTitle(QStringLiteral("Get Height"));
    setCentralWidget(m_testHeightWidget);
    m_testHeightWidget->show();

    connect(m_testHeightWidget, &TestHeightWidget::loadFileTriggered, this, &MainWindow::LoadFile);
    connect(m_testHeightWidget, &TestHeightWidget::startTestTriggered, this, &MainWindow::StartTest);
    connect(m_testHeightWidget, &TestHeightWidget::selectImageTriggered, this, &MainWindow::SelectImage);
    connect(m_testHeightWidget, &TestHeightWidget::SetPreferenceTriggered, this, &MainWindow::SetPreference);
}

void MainWindow::LoadFile() 
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
    qDebug() << "Processed image count:" << imageInfos.size();
    for (const auto& info : imageInfos) {
        qDebug() 
                 << "Spot1 Pos:(" << info.spot1Pos.x << ", " << info.spot1Pos.y << ")"
                 << "Spot2 Pos:(" << info.spot2Pos.x << ", " << info.spot2Pos.y << ")"
                 << "Distance Px:" << (info.distancePx.has_value() ? QString::number(info.distancePx.value()) : "N/A")
                 << "Distance Mm:" << (info.distancePx.has_value() ? QString::number(info.distancePx.value()*5.0/imageInfos[0].distancePx.value()) : "N/A")
                 << "Height Mm:" << (info.heightMm.has_value() ? QString::number(info.heightMm.value()) : "N/A")
                 << "-----------------------------------";
    }
}

//开始测高
void MainWindow::StartTest() 
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
void MainWindow::SelectImage() 
{
    if (!m_heightCore) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("测高引擎未初始化"));
        return;
    }
    if(!m_heightCore->loadTestImageInfo()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择有效的测试图片"));
        return;
    }
}
//显示当前图片高度
void MainWindow::SetPreference() 
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
    for (const auto& info : imageInfos) {
        qDebug() 
                 << "Spot1 Pos:(" << info.spot1Pos.x << ", " << info.spot1Pos.y << ")"
                 << "Spot2 Pos:(" << info.spot2Pos.x << ", " << info.spot2Pos.y << ")"
                 << "Distance Px:" << (info.distancePx.has_value() ? QString::number(info.distancePx.value()) : "N/A")
                 << "Distance Mm:" << (info.distancePx.has_value() ? QString::number(info.distancePx.value()*5.0/imageInfos[0].distancePx.value()) : "N/A")
                 << "Height Mm:" << (info.heightMm.has_value() ? QString::number(info.heightMm.value()) : "N/A")
                 << "-----------------------------------";
    }

    m_heightCore->getCalibrationLinear(calibA, calibB);
    qDebug() << "拟合直线:y = " << calibA << "x + " << calibB;
}

