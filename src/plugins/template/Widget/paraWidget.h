#pragma once
#include <QWidget>
#include "template/template_global.h"
#include "../../../interfaces/IMatchPlugin.h"


class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QToolButton;
class QComboBox;
class QSlider;
class QTimer;




class TEMPLATE_EXPORT ParaWidget : public QWidget {
    Q_OBJECT

public:
    ParaWidget(QWidget* parent = nullptr); 
    ~ParaWidget();
    MatchParams buildMatchParams();
    FindMatchParams buildFindMatchParams(bool useRoi);
private:
    void init();

private:
    QSpinBox* m_maxNumberSpinBox; // 最大匹配数量
    QSpinBox* m_learnFeatureSpinBox; // 最小特征点数量
    QComboBox* m_matchCenterComboBox; // 匹配中心选择
    QCheckBox* m_polarityCheckBox; // 是否忽略极性
    QCheckBox* m_subPixelCheckBox; // 是否启用严格评分
    QDoubleSpinBox* m_matchThresholdSpinBox; // 匹配阈值
    QDoubleSpinBox* m_matchScoreSpinBox; // 匹配分数阈值
    QDoubleSpinBox* m_matchAngleSpinBox; // 匹配角度范围
    QDoubleSpinBox* m_angleStepSpinBox; // 匹配角度步长
    QDoubleSpinBox* m_minScaleSpinBox; // 最小缩放比例
    QDoubleSpinBox* m_maxScaleSpinBox; // 最大缩放比例
    QDoubleSpinBox* m_scaleStepSpinBox; // 缩放步长
    QSpinBox* m_weakThresholdSpinBox; // 模板对比度滑块
    QSpinBox* m_strongThresholdSpinBox; // 匹配对比度滑块
};