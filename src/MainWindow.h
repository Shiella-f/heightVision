#pragma once
#include <QMainWindow>
#include <memory>
#include <optional>
#include "widget/heightMeature/Widget/testHWidget.h"
#include "widget/heightMeature/core/testHeight.h"
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();


private:
    void init();
    void createActions();

    void LoadFile();
    void StartTest();
    void SelectImage();
    void SetPreference();

private:
    TestHeightWidget* m_testHeightWidget;

    std::unique_ptr<Height::core::HeightCore> m_heightCore;
    QString m_loadedFolder;
    std::optional<size_t> m_selectedImageIndex;

    double calibA;
    double calibB;
};