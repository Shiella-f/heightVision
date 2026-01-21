#ifndef BSCV_MODULES_TEMPLMATCH_INCLUDE_TEMPLMATCH_H
#define BSCV_MODULES_TEMPLMATCH_INCLUDE_TEMPLMATCH_H

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

    class ITemplMatch
    {
    public:
        virtual ~ITemplMatch() = default;

        /*
            功能：生成模板。
            image：输入图像，它应该是一个8位的，单通道图像。
            mask：掩码图像，它应该是一个8位的，单通道图像。
            angle_start：旋转的起始角度。
            angle_extent：从起始角度开始旋转的范围。
            angle_step：旋转角度的步长。
            scale_min: 缩放，代表当前的最小缩放值。
            scale_max: 放大，代表当前的最大放大值。
            scale_step:缩放步长。
            weak_thresh：匹配阈值，在匹配时用在图片处理生成方向量化上，越高方向量化点越少。
            strong_thresh：模板阈值，生成模板时用来评价当前点是否为特征点，该值越高识别的特征点越少。
            num_features: 最小模板点数。
        */
        virtual bool create_shape_model(cv::InputArray _image, cv::InputArray _mask = cv::Mat(), float _angle_start = 0.0f, float _angle_extent = 360.0f, float _angle_step = 1.0f, float _scale_min = 0.9, float _scale_max = 1.1, float _scale_step = 0.01f,float _weak_thresh = 30.0f, float _strong_thresh = 60.0f, size_t _num_features = 64) = 0;
        virtual bool create_one_shape_model(cv::InputArray _image, cv::InputArray _mask = cv::Mat(), float _weak_thresh = 30.0f, float _strong_thresh = 60.0f, size_t _num_features = 30) = 0;

        /*
            功能：根据模板查找形状，输出匹配信息。
            image：输入图像，它应该是一个8位的，单通道图像。
            mask：掩码图像，它应该是一个8位的，单通道图像。
            threshhold：最低匹配分数阈值，高于此阈值的的match才有效，设置的越低越慢，最低0分，最高100分。
        */
        virtual std::vector<Match> find_shape_model(cv::InputArray _image, cv::InputArray _mask = cv::Mat(), float _threshold = 90.0f) = 0;

        /*
            功能：把model图像生成模板，然后根据模板再图像里面查找形状，输出匹配信息。
            image：输入图像，它应该是一个8位的，单通道图像。
            model: 输入的模板图像，，它应该是一个8位的，单通道图像。
            mask：掩码图像，它应该是一个8位的，单通道图像。
            model_mask:
            threshhold：最低匹配分数阈值，高于此阈值的的match才有效，设置的越低越慢，最低0分，最高100分。
            angle_start：旋转的起始角度。
            angle_extent：从起始角度开始旋转的范围。
            angle_step：旋转角度的步长。
            scale_min: 缩放，代表当前的最小缩放值。
            scale_max: 放大，代表当前的最大放大值。
            scale_step:缩放步长。
            weak_thresh：匹配阈值，在匹配时用在图片处理生成方向量化上，越高方向量化点越少。
            num_features: 最小模板点数。
        */
        virtual std::vector<Match> find_pic_model(cv::InputArray _image, cv::InputArray _model, cv::InputArray _mask = cv::Mat(), cv::InputArray _model_mask = cv::Mat(), float _threshold = 90.0f, float _angle_start = 0.0f, float _angle_extent = 360.0f, float _angle_step = 1.0f, float _scale_min = 0.9, float _scale_max = 1.1, float _scale_step = 0.01f,float _weak_thresh = 30.0f, size_t _num_features = 64) = 0;

        virtual Template getTempl(int p_id) const = 0;
        virtual float getAngle(int p_id) const = 0;
        virtual float getScale(int p_id) const = 0;

        virtual bool write_shape_model(const cv::String _fileName) const = 0;
        virtual bool read_shape_model(const cv::String _fileName) = 0;

        virtual bool isEmpty() const = 0;
        virtual void clear() = 0;

    protected:
        ITemplMatch() = default;
    };

    ITemplMatch *newTemplMatch();
    ITemplMatch *newTemplMatch(ITemplMatch * p_templMatch);
}

#endif  //BSCV_MODULES_TEMPLMATCH_INCLUDE_TEMPLMATCH_H