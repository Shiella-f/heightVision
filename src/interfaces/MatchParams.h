#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <QVector>
#include <QVariant>
#include <QMetaType>

enum class MatchCenterType {
    SceneCenter,   // 几何2中心 
    TrainCenter  // 训练中心
};
struct MatchParams {
    
    double  angleRange = 360;      // 匹配角度
    float angle_step = 1.0f;      // 角度步长
    size_t     FeaturePointNum = 128;    // 最小特征点数量

    float  weakThreshold = 60.0f; //模板对比度
    float  strongThreshold = 30.0f; //匹配对比度
    bool    ignorePolarity = false;

    float scale_min = 0.95f;
    float scale_max = 1.05f;
    float scale_step = 0.01f;

    // 为防止模板学习时将 ROI 边框当作特征点，支持忽略边界像素宽度（单位：px）
    // 当 >0 时会在模板学习阶段构造掩膜，屏蔽距离四边小于该阈值的区域
    int borderPadding = 10;
};

struct FindMatchParams
{
    int     maxCount = 10;// 最大数量
    bool    useSubPx = true;
    MatchCenterType centerType = MatchCenterType::SceneCenter;
    double  scoreThreshold = 0.7;  // 分数阈值(0~1)，界面显示 70.00
    bool    useRoi = false;         // true=ROI 内匹配，false=全图匹配
    cv::Mat Mask; // 模板掩码
};


struct MatchResult {
    cv::Rect      Roiarea;    // 矩形框
    double        score;   // 0~1, 界面显示 *100
    double        angle;   // 匹配角度
    double        scale;   // 匹配缩放
    cv::Point2f   center;  // 基准点
    cv::RotatedRect rotatedRect; // 最小外接矩形框
    cv::Size2f    featureSize{0.f, 0.f}; // 保存特征点最小外接矩形尺寸，便于按真实模板大小绘制
    std::vector<cv::Point2f> transformedFeaturePoints; // 该匹配下所有特征点的全图坐标
    //std::vector<std::vector<cv::Point2f>> contours; // 变换后的轮廓点
};
struct MatchResults
{
    QVector<MatchResult> results;
};

Q_DECLARE_METATYPE(MatchResults);

