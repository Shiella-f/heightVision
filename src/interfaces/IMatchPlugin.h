#pragma once
#include <QWidget>
#include <opencv2/core/mat.hpp>
#include "MatchParams.h"
#include "orionvisionglobal.h"

class IMatchWidget {
public:
    virtual ~IMatchWidget() {}
    
    virtual void show() = 0; 
    virtual bool setcurrentImage(const cv::Mat& image) = 0;
    virtual bool hasLearnedTemplate() const = 0;
    virtual QWidget* asWidget() = 0;
    virtual bool setinitData(const initOrionVisionParam& para) = 0; // 初始化参数
    virtual std::vector<MatchResult> getMatchResults() const = 0;// 获取匹配结果
    // 设置振镜校准矩阵
    virtual bool setMirrorCalibMatrix(const cv::Mat& mat) = 0;
    // 图像点转换为物理点
    virtual std::vector<cv::Point2f> getPhysicalPoint(const std::vector<cv::Point2f> imagePoint) = 0;
    // 获取移动和旋转数据
    virtual CopyItems_Move_Rotate_Data getMoveRotateData() const = 0;
    // 获取移动和旋转数据的 QVariantMap 表示
    virtual QVariantMap getMoveRotateDataMap() const = 0;
    // 获取路径
    virtual QVector<QPainterPath> getPaths() const = 0;
signals:
    virtual void sendDrawPath() = 0; // 匹配完成信号

};

// 插件工厂接口
class IMatchPluginFactory {
public:
    virtual ~IMatchPluginFactory() {}
    // 创建匹配窗口实例
    virtual IMatchWidget* createMatchWidget(QWidget* parent = nullptr) = 0;
};

#define IMatchPluginFactory_iid "com.bcadhicv.IMatchPluginFactory"
Q_DECLARE_INTERFACE(IMatchPluginFactory, IMatchPluginFactory_iid)
