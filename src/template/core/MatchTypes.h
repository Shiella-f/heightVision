#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

enum class MatchCenterType {
    RoiCenter,   // 框选中心
    TrainCenter  // 训练中心
};

struct MatchParams {
    int     maxCount = 1;          // 最大数量
    double  angleRange = 5.0;      // 匹配角度 ±5°
    double  scoreThreshold = 0.7;  // 分数阈值(0~1)，界面显示 70.00
    int     learningDetail = 5;    // 学习细节
    MatchCenterType centerType = MatchCenterType::RoiCenter;
    bool    ignorePolarity = false;
    bool    strictScore = true;
    bool    useRoi = true;         // true=ROI 内匹配，false=全图匹配
};

struct MatchResult {
    cv::Rect      Roiarea;    // 原图坐标系中的矩形框
    double        score;   // 0~1, 界面显示 *100
    double        angle;   // 匹配角度
    cv::Point2f   center;  // 基准点
};