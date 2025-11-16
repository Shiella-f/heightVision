#include "TemplateManager.h"

bool TemplateManager::learnTemplate(const cv::Mat& src, const cv::Rect& roi, const MatchParams& params)
{
    if (src.empty()) return false;
    cv::Rect validRoi = roi & cv::Rect(0, 0, src.cols, src.rows);
    if (validRoi.width <= 0 || validRoi.height <= 0) return false;

    m_template = src(validRoi).clone();
    m_learnParams = params;
    m_trainCenter = cv::Point2f(m_template.cols / 2.0f, m_template.rows / 2.0f);
    m_valid = true;
    return true;
}