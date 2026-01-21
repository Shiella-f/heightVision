#pragma once
#include <QObject>
#include <QtPlugin>
#include <QPointer>
#include "../interfaces/HeightPluginInterface.h"
#include "Widget/HeightMainWindow.h"

class HeightMeaturePlugin : public QObject, public HeightPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.yourcompany.PluginInterface" FILE "height_meature_plugin.json")
    Q_INTERFACES(HeightPluginInterface)

public:
    QString name() const override { return "HeightMeature"; }
    QWidget* createWidget(QWidget* parent = nullptr) override {
        m_widget = new HeightMainWindow(parent);
        return m_widget;
    }

    void loadFile() override {
        if (m_widget) m_widget->LoadFile();
    }

    void startTest() override {
        if (m_widget) m_widget->StartTest();
    }

    void selectImage() override {
        if (m_widget) m_widget->SelectImage();
    }

    void setPreference() override {
        if (m_widget) m_widget->SetPreference();
    }

    void saveCalibrationData() override {
        if (m_widget) m_widget->onCalibrationDataSaved();
    }

    void loadCalibrationData() override {
        if (m_widget) m_widget->onCalibrationDataLoaded();
    }

    double getMeasurementResult() const override {
        if (m_widget) return m_widget->getMeasurementResult();
        return -1.0;
    }

    void setCameraImage(const cv::Mat& image) override {
        if (m_widget) m_widget->setCameraImage(image);
    }

private:
    // 使用 QPointer 自动处理悬空指针，防止 widget 被销毁后访问崩溃
    QPointer<HeightMainWindow> m_widget;
};
