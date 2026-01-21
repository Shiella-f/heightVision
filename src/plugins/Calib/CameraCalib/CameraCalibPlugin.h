#pragma once
#include <QObject>
#include <QWidget>
#include <QPointer>
#include "../interfaces/CalibInterface.h"
#include "CameraCalib/Widget/CameraCalibWidget.h"
// 工厂接口：为外部项目提供相机标定界面实例
class ICameraCalibPluginFactory : public QObject , public CameraCalibInterface {
    Q_OBJECT
    // moc 需要 IID 是字符串字面量，否则会在解析阶段报 "Parse error at \"IID\""
    Q_PLUGIN_METADATA(IID "com.yourcompany.CameraCalibInterface" FILE "CameraCalib_plugin.json")
    Q_INTERFACES(CameraCalibInterface)
public:
    QString name() const override {
        return QStringLiteral("CameraCalibPlugin");
    }

    QWidget* createWidget(QWidget* parent = nullptr) override {
        m_widget = new CameraCalibWidget(parent);
        return m_widget;
    }
private:
    QPointer<CameraCalibWidget> m_widget;

};

