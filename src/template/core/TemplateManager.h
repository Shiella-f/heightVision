#pragma once
#include "MatchTypes.h"

class TemplateManager {
public:
    bool learnTemplate(const cv::Mat& src, const cv::Rect& roi, const MatchParams& params);
    bool hasTemplate() const { return m_valid; }

    const cv::Mat& templateImage() const { return m_template; }
    const MatchParams& learningParams() const { return m_learnParams; }
    cv::Point2f trainCenter() const { return m_trainCenter; }

private:
    cv::Mat      m_template;
    MatchParams  m_learnParams;
    cv::Point2f  m_trainCenter{0.f, 0.f};
    bool         m_valid = false;
};