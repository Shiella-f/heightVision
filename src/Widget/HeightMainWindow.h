#pragma once
#include <QWidget>
#include <memory>
#include <optional>
#include <opencv2/opencv.hpp>
#include "heightMeature/Widget/testHWidget.h"
#include "heightMeature/core/testHeight.h"
#include "tools/bscvTool.h"

class HeightMainWindow : public QWidget {
    Q_OBJECT

public:
    HeightMainWindow(QWidget* parent = nullptr);
    ~HeightMainWindow();


private:
    void init();
    void createActions();

    void LoadFile();
    void StartTest();
    void SelectImage();
    void SetPreference();
private slots:
    void GetROI(const QRectF& roi);

private:
    TestHeightWidget* m_testHeightWidget;

    std::unique_ptr<Height::core::HeightCore> m_heightCore;
    QString m_loadedFolder;
    std::optional<size_t> m_selectedImageIndex;

    double calibA;
    double calibB;
    cv::Mat m_showImage = cv::Mat();

    double m_TestHeight = -1.0;

    cv::Rect2f m_cvRoi;
};