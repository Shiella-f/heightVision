#pragma once
#include "MatchTypes.h"

class TemplateManager;
class Matcher {
public:
    std::vector<MatchResult> match(
        const cv::Mat& src,
        const TemplateManager& tmplMgr,
        const MatchParams& params,
        const cv::Rect& roiRect
    );
};