#pragma once
#include <QWidget>

class ZoomScene;
class QToolButton;
class QLabel;
class QGroupBox;
class SceneImageItem;

class TestHeightWidget : public QWidget {
    Q_OBJECT

public:
    explicit TestHeightWidget(QWidget *parent = nullptr);
    ~TestHeightWidget();

    void init();

signals:
    void loadFileTriggered();
    void startTestTriggered();
    void selectImageTriggered();
    void SetPreferenceTriggered();

protected:

private:
    QToolButton* m_loadFileBtn;
    QToolButton* m_startTestBtn;
    QToolButton* m_selectImageBtn;
    QToolButton* m_SetPreferenceBtn;

    QLabel* m_currentHeightLabel;
    QLabel* m_resultLabel;

    ZoomScene* m_zoomScene;
    SceneImageItem* m_sceneImageItem;
    QGroupBox* m_testAreaGroupBox;
    QWidget* m_testHeightWidget;
};
