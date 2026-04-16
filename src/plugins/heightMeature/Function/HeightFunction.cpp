#include "HeightFunction.h"
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

HeightFunction::HeightFunction()
{
    m_heightCore = std::make_unique<Height::core::HeightCore>();
    HeightScene* heightScene = new HeightScene(this);
    setScene(heightScene);
}

HeightFunction::~HeightFunction() = default;

bool HeightFunction::isCalibrated() const
{
    return m_heightCore && m_heightCore->hasLinearCalib();
}

void HeightFunction::displayImage(const cv::Mat &image)
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
        m_zoomScene->showPlaceholder(QStringLiteral("Convert Failed"));
        return;
    }

    m_zoomScene->showImage(qImage);
}

bool HeightFunction::LoadFolder(const QString& path)
{
    if (!m_heightCore) {
        m_heightCore = std::make_unique<Height::core::HeightCore>();
    }

    if (path.isEmpty()) {
        return false;
    }

    const bool success = m_heightCore->loadFolder(path);
    if (!success) {
        return false;
    }
    
    auto imageInfos = m_heightCore->getImageInfos();
    m_showImage = m_heightCore->getShowImage();
    displayImage(m_showImage);
    return true;
}
//开始测高
double HeightFunction::StartTest() 
{
    if (!m_heightCore) {
        return -1.0;
    }
    GetROI();
    if(m_cvRoi.width <= 0 || m_cvRoi.height <= 0) {
        m_cvRoi = cv::Rect2f(0, 0, CameraImg.cols, CameraImg.rows);
        m_heightCore->setROI(m_cvRoi);
    }
    if(!m_heightCore->computeTestImageInfo()) {
        return -1.0;
    }
    m_TestHeight = m_heightCore->measureHeightForImage().value_or(-1.0);
    if (m_TestHeight < 0.0) {
        return -1.0;
    }
    return m_TestHeight;
}
//选择图片显示测高
bool HeightFunction::SelectImage(const QString &imagePath)
{
    if (!m_heightCore) {
        return false;
    }

    if (imagePath.isEmpty()) {
        return false;
    }

    if(!m_heightCore->loadTestImageInfo(imagePath)) {
        return false;
    }
    m_showImage = m_heightCore->getShowImage();
    displayImage(m_showImage);
    return true;
}
//设置参考高度
bool HeightFunction::SetPreference(const QString& preferenceImagePath, double height)
{
    GetROI();
    if(m_cvRoi.width <= 0 || m_cvRoi.height <= 0) {
        m_cvRoi = cv::Rect2f(0, 0, CameraImg.cols, CameraImg.rows);
        m_heightCore->setROI(m_cvRoi);
    }
    if (!m_heightCore) {
        return false;
    }

    if (preferenceImagePath.isEmpty())
        return false;

    if(!m_heightCore->importImageAsReference(preferenceImagePath)) {
        return false;
    }

    double prefHeight = height;
    if(prefHeight < 0.0) return false;

    if(!m_heightCore->setPreferenceHeight(prefHeight)) {
        return false;
    }
    if(!m_heightCore->computeHeightValues()) {
        return false;
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
    return true;
}
//获取ROI
void HeightFunction::GetROI() 
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
}

bool HeightFunction::SaveCalibrationData(const QString& savePath)
{
    if (!m_heightCore) {
        return false;
    }

    if (savePath.isEmpty()) {
        return false;
    }

    if (!m_heightCore->saveCalibrationData(savePath)) {
        return false;
    }
    return true;
}

bool HeightFunction::LoadCalibrationData(const QString& loadPath)
{
    if (!m_heightCore) {
        return false;
    }

    if (loadPath.isEmpty()) {
        return false;
    }

    if (!m_heightCore->loadCalibrationData(loadPath)) {
        return false;
    }

    m_heightCore->setIsLinearCalib(true);
    m_heightCore->getCalibrationLinear(calibA, calibB);
    return true;
}

double HeightFunction::getMeasurementResult() const
{
    return m_TestHeight;
}

IHeightScene* HeightFunction::getScene() const
{
    return m_zoomScene;
}


bool HeightFunction::setScene(HeightScene* scene) {
    if (!scene) {
        return false;
    }
    m_zoomScene = scene;
    return true;
}

bool HeightFunction::SelectROI() {
    if(m_zoomScene) 
    {
        m_zoomScene->setRoiSelectionEnabled(true);
        return true;
    }
    return false;
}

bool HeightFunction::ConfirmROI() {
    if(m_zoomScene) {
        QRectF roi;
        m_zoomScene->getRoiArea(roi);
        GetROI();
        return true;
    }
    return false;
}

IHeightMeasure* newIHeightMeasure() {
    return new HeightFunction();
}
