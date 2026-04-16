#pragma once
#include <QString>
#include <QRectF>
#include <QImage>
#include "template/template_global.h"
#include "../core/MatchParams.h"
#include "../core/orionvisionglobal.h"

class IMatchScene {
public:
    virtual ~IMatchScene() = default;
    enum CompositionMode { United = 0, Intersected = 1, Subtracted = 2 };
    enum ShapeMode { Rect = 0, Ellipse = 1, Free = 2 };

    virtual void setSelectionTemplateEnabled(bool enable) = 0;
    virtual bool showImage(const QImage& image) = 0;
    //设置组合模式（联合、交集、差集）
    virtual bool setCompositionMode(CompositionMode m) = 0;
    //设置形状模式（矩形、椭圆、自由）
    virtual bool setShapeMode(ShapeMode s) = 0;
    //设置绘制ROI的画笔宽度
    virtual bool setPenWidth(int w) = 0;
protected:
    IMatchScene() = default;
};

class Match{
    public:
    virtual ~Match() = default;
    virtual bool setinitData(const initOrionVisionParam& para) = 0; // 初始化路径参数
    // 执行匹配并返回结果,执行前需先设置参数和创建模板；在Scene中选取ROI后会自动创建模板
    virtual std::vector<MatchResult> confirmMatch() = 0;
    virtual bool buildMatchParams(MatchParams& params) = 0;// 构建模板参数，返回是否成功
    virtual bool buildFindParams(FindMatchParams& params) = 0;// 构建匹配参数，返回是否成功
    virtual CopyItems_Move_Rotate_Data getMoveRotateData() = 0;// 获取移动和旋转数据
    virtual void clearToggled() = 0;//清空场景Item
    virtual void ShowPath() = 0;//在Scene中显示路径
    virtual IMatchScene* getScene() const = 0;//返回场景指针
protected:
    Match() = default;
};

TEMPLATE_EXPORT Match* newMatch();

