#pragma once
#include <memory>
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
    bool learnTemplate(const cv::Mat& src, const cv::Mat& templateMat, const MatchParams& params);
    std::vector<MatchResult> matchTemplate(const cv::Mat& src, const FindMatchParams& params) const;
    bool hasTemplate() const { return m_valid; }

    std::vector<cv::Point2f> currentFeaturePoints() const;

    const cv::Mat& templateImage() const { return m_template; }
    const MatchParams& learningParams() const { return m_learnParams; }
    cv::Point2f trainCenter() const { return m_trainCenter; }

private:
    static std::vector<cv::Point2f> collectFeaturePoints(const TIGER_BSVISION::Template& templ);
    static cv::Rect2f computeFeatureBounds(const std::vector<cv::Point2f>& points);

private:
    cv::Mat      m_template;
    MatchParams  m_learnParams;
    cv::Point2f  m_trainCenter{0.f, 0.f};
    std::vector<cv::Point2f> m_featurePoints;
    cv::Rect2f   m_featureBounds{0.f, 0.f, 0.f, 0.f}; // 记录模板特征点的最小外接矩形，便于后续输出真实尺寸
    bool         m_valid = false;
    std::unique_ptr<TIGER_BSVISION::ITemplMatch> m_matchEngine;
};