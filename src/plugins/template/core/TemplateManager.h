#pragma once
#include <memory>
#include <array>
#include <vector>
#include "MatchParams.h"
#include <bscv/templmatch.h>
#include <fstream>
#include <algorithm>
#include "template/template_global.h"
#include <opencv2/imgproc.hpp>

class TEMPLATE_EXPORT TemplateManager {
public:
    TemplateManager();
    ~TemplateManager();
    //获取模板   src: 输入图像   templateMat: 模板图像   params: 模板参数
    bool learnTemplate(const cv::Mat& src, const cv::Mat& templateMat, const MatchParams& params);
    //匹配模板，返回所有匹配结果   src: 输入图像    params: 匹配参数
    std::vector<MatchResult> matchTemplate(const cv::Mat& src, const FindMatchParams& params) const;
    bool hasTemplate() const { return m_valid; }
    //返回当前模板的特征点坐标
    std::vector<cv::Point2f> currentFeaturePoints() const{ return m_featurePoints;}
    cv::Point2f trainCenter() const { return m_trainCenter; }
private:
    //设置当前模板的特征点坐标
    static std::vector<cv::Point2f> collectFeaturePoints(const TIGER_BSVISION::Template& templ);
    //计算最小外接矩形
    static cv::Rect2f computeFeatureBounds(const std::vector<cv::Point2f>& points);

private:
    cv::Mat      m_template;
    MatchParams  m_learnParams;
    cv::Point2f  m_trainCenter{0.f, 0.f};
    cv::Point2f  m_engineTrainCenter{0.f, 0.f};
    std::vector<cv::Point2f> m_featurePoints;
    cv::Rect2f   m_featureBounds{0.f, 0.f, 0.f, 0.f}; // 记录模板特征点的最小外接矩形，便于后续输出真实尺寸
    int          m_pyramidLevel = 0;
    float        m_pyramidScale = 1.0f;
    // 轮询金字塔 0~3：每个槽位对应一个请求层级（0/1/2/3），effectiveLevel 可能因图像过小被截断。
    std::array<std::unique_ptr<TIGER_BSVISION::ITemplMatch>, 4> m_matchEngines;
    std::array<cv::Point2f, 4> m_engineTrainCenters{{cv::Point2f(0.f, 0.f), cv::Point2f(0.f, 0.f), cv::Point2f(0.f, 0.f), cv::Point2f(0.f, 0.f)}};
    std::array<int, 4> m_effectiveLevels{{0, 0, 0, 0}};
    bool         m_valid = false;
};