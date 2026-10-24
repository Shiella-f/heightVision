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
#include "../core/MatchParams.h"
#include "../core/orionvisionglobal.h"
//#include "template/core/client/UDPMatReceiver.h"
#include "../camera/ImageProcess.h"
#include "matchPort.h"

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

class TEMPLATE_EXPORT matchFunction : public QObject, public Match {
    Q_OBJECT

public:
    matchFunction(QWidget* parent = nullptr); 
    ~matchFunction();

    std::vector<MatchResult> confirmMatch() override;
    void clearToggled() override;
    
    void ShowPath() override;
    IMatchScene* getScene() const override;
    bool buildMatchParams(MatchParams& params) override;
    bool buildFindParams(FindMatchParams& params) override;
    bool setinitData(const initOrionVisionParam& para) override;
    CopyItems_Move_Rotate_Data getMoveRotateData() override;

    bool hasLearnedTemplate() const;
    bool setMoveRotateData();
    

    
public:
    virtual QVariantMap getMoveRotateDataMap() const;

    std::vector<MatchResult> getMatchResults() const {return m_lastMatchResults;}

    QVector<QPainterPath> getPaths() const {
        return m_drawPaths;
    }

private:
    bool setcurrentImage(const cv::Mat& image); // 设置当前用于显示与匹配的图像
    bool drawPath();
    bool learnMatch();
    bool confirmDraw();

    void executeMatch(bool useRoi);
    void renderMatchResults(const std::vector<MatchResult> &results);
    QVector<QPainterPath> copyPainterPathWithTransform(const QPainterPath& originalPath,
                                                      const QVector<QPointF>& moveOffsets,
                                                      const QVector<double>& angles,
                                                       QPointF pivotPoint);
    void scaleinitData();

    //void connectUDPReceiver();

protected:
    

private:
    
    MatchScene* m_ImageDisplayScene; // 主场景

    cv::Mat m_currentImage;
    TemplateManager m_templateManager;

    cv::Mat m_learnedTemplate; // 学习后裁剪得到的模板图像
    std::vector<cv::Point2f> m_learnedFeaturePoints; // 模板上的特征点集合
    std::vector<cv::KeyPoint> m_learnedKeypoints; // 模板上的关键点集合
    cv::Mat m_learnedDescriptors; // 模板关键点的描述子矩阵
    bool m_hasLearnedTemplate = false; // 标记是否已经学习到模板

    bool m_TemplateAreaConfirmed = false; // 标记模板区域是否已被用户确认
    

    MatchParams m_MatchParams;
    FindMatchParams m_FindMatchParams;
    std::vector<MatchResult> m_lastMatchResults; // 保存上一次匹配结果

    ImageProcess* m_ImageProcess;

    CopyItems_Move_Rotate_Data m_MoveRotateData;

    initOrionVisionParam m_initParam;
    initOrionVisionParam m_cale_initParam;
    QPointF initPoint;
    QVector<QPainterPath> m_drawPaths;

    double m_scaleX;
    double m_scaleY;
};
