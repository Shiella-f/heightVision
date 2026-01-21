#pragma once

#include <QObject>
#include "../../../interfaces/IMatchPlugin.h"
#include "Widget/matchWidget.h"

// 插件工厂实现类
class TemplatePlugin : public QObject, public IMatchPluginFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IMatchPluginFactory_iid FILE "template_plugin.json")
    Q_INTERFACES(IMatchPluginFactory)

public:
    // 创建匹配窗口实例
    IMatchWidget* createMatchWidget(QWidget* parent = nullptr) override {
        return new matchWidget(parent);
    }
};
