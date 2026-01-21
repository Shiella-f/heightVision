#pragma once
#include <QObject>
#include <QWidget>
#include <QPointer>
#include "../interfaces/MirrorCalibInterface.h"
#include "Widget/MirrorCalibWidget.h"

class IMirrorCalibPluginFactory : public QObject , public MirrorCalibInterface {
    Q_OBJECT
    // moc 需要 IID 是字符串字面量，否则会在解析阶段报 "Parse error at \"IID\""
    Q_PLUGIN_METADATA(IID "com.yourcompany.MirrorCalibInterface" FILE "mirrorcalib_plugin.json")
    Q_INTERFACES(MirrorCalibInterface)
public:
    QString name() const override {
        return QStringLiteral("MirrorCalibPlugin");
    }

    QWidget* createWidget(QWidget* parent = nullptr) override {
        m_widget = new MirrorCalibWidget(parent);
        return m_widget;
    }
private:
    QPointer<MirrorCalibWidget> m_widget;
};
