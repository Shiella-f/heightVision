#pragma once
#include <QWidget>
#include <opencv2/opencv.hpp>
#include <memory>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QString>

#include "tools/bscvTool.h"
#include "template/core/TemplateManager.h"
#include "template/template_global.h"
#include "../../../interfaces/IMatchPlugin.h"
#include "../camera/camera.h"
#include "../../../interfaces/orionvisionglobal.h"
#include "../../../interfaces/CalibInterface.h"
#include "../../../interfaces/HeightPluginInterface.h"
#include "paraWidget.h"
#include "template/core/client/UDPMatReceiver.h"
#include "../camera/ImageProcess.h"
#include "../common/Widget/baseWidget.h"
#include "../common/Widget/CustomTitleBar.h"

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QToolButton;
class QComboBox;
class ImageSceneBase;
class MatchScene;
class QSlider;
class QTimer;
class QScrollArea;

class TEMPLATE_EXPORT matchWidget : public baseWidget, public IMatchWidget {
    Q_OBJECT

public:
    matchWidget(QWidget* parent = nullptr); 
    ~matchWidget();

    void show() override { QWidget::show(); }
    QWidget* asWidget() override { return this; }

    bool setcurrentImage(const cv::Mat& image) override; // 设置当前用于显示与匹配的图像
    bool hasLearnedTemplate() const override;
    bool setMoveRotateData();
    void connectUDPReceiver();
    
public:
    virtual bool setinitData(const initOrionVisionParam& para) override {
        m_initParam = para;
        return true;
    }
    
    virtual CopyItems_Move_Rotate_Data getMoveRotateData() const { return m_MoveRotateData; }
    virtual QVariantMap getMoveRotateDataMap() const;

    virtual std::vector<MatchResult> getMatchResults() const {return m_lastMatchResults;}

    virtual bool setMirrorCalibMatrix(const cv::Mat& mat) {
        if(mat.empty()){
            return false;
        }
        HomographyMatrix = mat;
        m_isCalibLoaded_Mirror = true;
        return true;
    }

    virtual QVector<QPainterPath> getPaths() const override {
        return m_drawPaths;
    }

    virtual std::vector<cv::Point2f> getPhysicalPoint(const std::vector<cv::Point2f> imagePoint);

signals:
    virtual void sendDrawPath(); // 匹配完成信号

private slots:
    void clearToggled();
    bool learnMatch();
    void confirmMatch(); // 在整幅图像上进行匹配并显示结果
    void OpenImage();
    void onMatchFinished();
    void DrawPathBtnClicked();
    bool confirmDrawBtnClicked();

    void CameraCalibBtnClicked();
    void testHeightBtnClicked();

private:
    void init();
    void executeMatch(bool useRoi);
    void buildMatchParams();
    void buildFindMatchParams(bool useRoi);

    void applyLoadedStyleSheet(QWidget* widget);

    void renderMatchResults(const std::vector<MatchResult> &results);
    QVector<QPainterPath> copyPainterPathWithTransform(const QPainterPath& originalPath,
                                                      const QVector<QPointF>& moveOffsets,
                                                      const QVector<double>& angles,
                                                       QPointF pivotPoint);
    void scaleinitData();

    void loadHeightPlugin(); // 加载测高插件
    void loadCalibPlugin(); // 加载标定插件

protected:
    

private:
    QToolButton* m_confirmMatchBtn;
    QToolButton* m_clearBtn;
    QToolButton* m_openImageBtn; 
    QToolButton* m_connectUDPBtn;
    QToolButton* m_loadcalibBtn;
    QToolButton* m_drawpathBtn;
    QToolButton* m_confirmDrawBtn;

    QToolButton* m_CameraCalibBtn;
    QToolButton* m_testHeightBtn;
    QToolButton* m_testBtn;

    QWidget* m_heightMainWindow = nullptr;
    HeightPluginInterface* m_heightPluginFactory = nullptr;

    QWidget* m_CameraCalibMainWindow = nullptr;
    CalibInterface* m_cameraCalibPluginFactory = nullptr;

    QWidget* m_mirrorCalibMainWindow = nullptr;

    QString m_loadedStyleSheet;

    QWidget* showParaWidget; // 主控件区域
    QScrollArea* paraScrollArea;
    ParaWidget* m_paraWidget; // 参数设置面板

    // 图像显示控件与场景
    QWidget* m_imageDisplayWidget; // 主图像控件
    MatchScene* m_ImageDisplayScene; // 主场景
    QWidget* m_roiDisplayWidget; // 右侧场景控件
    ImageSceneBase* m_RoiDisplayScene; // 右侧预览场景

    QComboBox* m_shapeModeCombo = nullptr;
    QComboBox* m_compModeCombo = nullptr;
    QSpinBox* m_penWidthSpin = nullptr;
    QCheckBox* m_paraWidgetshow; // 是否显示参数设置面板
    QCheckBox* m_matchselectCheckBox; // 是否模板选择


    cv::Mat m_currentImage;
    TemplateManager m_templateManager;

    cv::Mat m_learnedTemplate; // 学习后裁剪得到的模板图像
    std::vector<cv::Point2f> m_learnedFeaturePoints; // 模板上的特征点集合
    std::vector<cv::KeyPoint> m_learnedKeypoints; // 模板上的关键点集合
    cv::Mat m_learnedDescriptors; // 模板关键点的描述子矩阵
    bool m_hasLearnedTemplate = false; // 标记是否已经学习到模板

    bool m_TemplateAreaConfirmed = false; // 标记模板区域是否已被用户确认
    
    // 异步匹配
    QFutureWatcher<std::vector<MatchResult>> m_matchWatcher;

    MatchParams m_MatchParams;
    FindMatchParams m_FindMatchParams;
    std::vector<MatchResult> m_lastMatchResults; // 保存上一次匹配结果

    ImageProcess* m_ImageProcess;

    CopyItems_Move_Rotate_Data m_MoveRotateData;

    MyCamera m_camera;
    cv::Mat HomographyMatrix;
    bool m_isCalibLoaded_Mirror = false;

    initOrionVisionParam m_initParam;
    initOrionVisionParam m_cale_initParam;
    QPointF initPoint;
    QVector<QPainterPath> m_drawPaths;

    double m_scaleX;
    double m_scaleY;
};