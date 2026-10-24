#ifndef BSCV_MODULES_TEMPLMATCH_INCLUDE_TEMPLMATCHTYPES_H
#define BSCV_MODULES_TEMPLMATCH_INCLUDE_TEMPLMATCHTYPES_H

#include "opencv2/core.hpp"
namespace TIGER_BSVISION
{
    struct Feature
    {
        int x;
        int y;
        int label;

        Feature() : x(0), y(0), label(0) {};
        Feature(int _x, int _y, int _label) : x(_x), y(_y), label(_label){};
    };

    struct Template
    {
        int width;
        int height;
        int tl_x;
        int tl_y;
        int pyramid_level;
        std::vector<Feature> features;
    };

    struct Match
    {
        int x, y;
        float xPrecise, yPrecise;
        double angle, scale;
        std::vector<cv::Point2f> pts;

        float similarity;
        std::string class_id;
        int template_id;

        Template templ;
        double init_angle, init_scale;
        int width, height;
        cv::Mat mat = cv::Mat(3, 3, CV_32F);

        cv::Point2f transPt(cv::Point2f p_pt) const;
        std::vector<cv::Point2f> transPts(std::vector<cv::Point2f> p_pts) const;

        Match() {};
        Match(int _x, int _y, float _xPrecise, float _yPrecise, double _angle, double _scale,  std::vector<cv::Point2f> _pts, float _similarity, const std::string &_class_id, int _template_id, const Template &_templ, double _init_angle, double _init_scale, int _width, double _height, const cv::Mat &_mat)
            : x(_x), y(_y), xPrecise(_xPrecise), yPrecise(_yPrecise), angle(_angle), scale(_scale), pts(_pts), similarity(_similarity), class_id(_class_id), template_id(_template_id), templ(_templ), init_angle(_init_angle), init_scale(_init_scale), width(_width), height(_height),mat(_mat) {};
        Match(int _x, int _y, float _xPrecise, float _yPrecise, double _angle, double _scale,  std::vector<cv::Point2f> _pts, float _similarity, const std::string &_class_id, int _template_id)
            : x(_x), y(_y), xPrecise(_xPrecise), yPrecise(_yPrecise), angle(_angle), scale(_scale), pts(_pts), similarity(_similarity), class_id(_class_id), template_id(_template_id) {};
    };
};

#endif  //BSCV_MODULES_TEMPLMATCH_INCLUDE_TEMPLMATCHTYPES_H
