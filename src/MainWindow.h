#pragma once
#include <QWidget>
#include "tools/bscvTool.h"
#include "Widget/HeightMainWindow.h"
#include "template/Widget/matchWidget.h"

class MainWindow : public QWidget {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();


private:
    void init();


private:
    matchWidget* m_matchWidget;
   
};