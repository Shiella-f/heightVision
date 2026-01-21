#pragma once
#include <QtPlugin>
#include <QWidget>
#include <QString>

class CalibInterface {
public:
    virtual ~CalibInterface() = default;
    virtual QString name() const = 0;
    virtual QWidget* createWidget(QWidget* parent = nullptr) = 0;
};

#define CalibInterface_iid "com.yourcompany.CalibInterface"
Q_DECLARE_INTERFACE(CalibInterface, CalibInterface_iid)