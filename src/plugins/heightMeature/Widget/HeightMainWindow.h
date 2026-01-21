#pragma once
#include <QWidget>
#include <QtCore/qglobal.h>
#include <memory>
#include <optional>
#include <opencv2/opencv.hpp>
#include "core/testHeight.h"
#include "../common/Widget/CustomTitleBar.h"

#if defined(HEIGHTMEATURE_LIBRARY)
#  define HEIGHTMEATURE_EXPORT Q_DECL_EXPORT // 构建 DLL 时导出符号
#else
#  define HEIGHTMEATURE_EXPORT Q_DECL_IMPORT // 使用 DLL 时导入符号
#endif

class HeightScene;
class QToolButton;
class QLabel;
class QGroupBox;
class QCheckBox;
class QSpinBox;
class QTimer;

class HEIGHTMEATURE_EXPORT HeightMainWindow : public QWidget {
    Q_OBJECT

public:
    HeightMainWindow(QWidget* parent = nullptr);
    ~HeightMainWindow();

    Q_INVOKABLE bool isCalibrated() const;
    
    // 新增公共接口
    void LoadFile();
    void StartTest();
    void SelectImage();
    void SetPreference();
    void onCalibrationDataSaved();
    void onCalibrationDataLoaded();
    double getMeasurementResult() const;
    void setCameraImage(const cv::Mat& image){CameraImg = image;}

private:
    void init();
    void displayImage(const cv::Mat &image);
    void setHeightdisplay(const double& height);
    void GetROI();
private slots:
    void OpenCameraBtnToggled();

private:
    QToolButton* m_loadFileBtn;
    QToolButton* m_startTestBtn;
    QToolButton* m_selectImageBtn;
    QToolButton* m_SetPreferenceBtn;
    QToolButton* m_selectROIBtn;
    QToolButton* m_confirmROIBtn;
    QToolButton* m_saveCalibrationDataBtn;
    QToolButton* m_loadCalibrationDataBtn;
    QToolButton* m_OpenCameraBtn;

    QCheckBox* m_isDisplayprocessImg;
    QSpinBox* m_thresholdBox;

    QLabel* m_currentHeightLabel;
    QLabel* m_resultLabel;

    HeightScene* m_zoomScene;
    QGroupBox* m_testAreaGroupBox;

    std::unique_ptr<Height::core::HeightCore> m_heightCore;
    QString m_loadedFolder;
    std::optional<size_t> m_selectedImageIndex;

    double calibA;
    double calibB;
    cv::Mat m_showImage = cv::Mat();
    cv::Mat CameraImg = cv::Mat();

    double m_TestHeight = -1.0;

    cv::Rect2f m_cvRoi;
    QTimer* m_CameraImgShowTimer = nullptr;
};