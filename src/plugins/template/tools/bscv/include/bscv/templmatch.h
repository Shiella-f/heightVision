#ifndef BSCV_MODULES_TEMPLMATCH_INCLUDE_TEMPLMATCH_H
#define BSCV_MODULES_TEMPLMATCH_INCLUDE_TEMPLMATCH_H

#include "templMatchTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace TIGER_BSVISION
{
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
            threshold_value:内外轮廓二值化阈值。
            threshold_type:0表示不区分，1表示大于阈值区域为有效区域，2表示小于阈值区域为有效区域。
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

    namespace templmatch
    {
        enum class ErrorCode
        {
            Ok = 0,
            EmptyImage,
            EmptyModel,
            InvalidArgument,
            CreateModelFailed,
            FindModelFailed,
            ReadModelFailed,
            WriteModelFailed,
            AlgorithmError
        };

        struct OperationResult
        {
            bool ok{false};
            ErrorCode code{ErrorCode::Ok};
            std::string message{};

            static OperationResult success();
            static OperationResult failure(ErrorCode code, const std::string& message);
        };

        template <typename T>
        struct Result
        {
            bool ok{false};
            ErrorCode code{ErrorCode::Ok};
            std::string message{};
            T value{};

            static Result<T> success(const T& value)
            {
                Result<T> result;
                result.ok = true;
                result.code = ErrorCode::Ok;
                result.value = value;
                return result;
            }

            static Result<T> failure(ErrorCode code, const std::string& message)
            {
                Result<T> result;
                result.ok = false;
                result.code = code;
                result.message = message;
                return result;
            }
        };

        struct ShapeModelOptions
        {
            float angle_start{0.0f};
            float angle_extent{360.0f};
            float angle_step{1.0f};
            float scale_min{0.9f};
            float scale_max{1.1f};
            float scale_step{0.01f};
            float weak_thresh{30.0f};
            float strong_thresh{60.0f};
            size_t num_features{64};
        };

        struct OneShapeModelOptions
        {
            float weak_thresh{30.0f};
            float strong_thresh{60.0f};
            size_t num_features{30};
        };

        struct FindOptions
        {
            float threshold{90.0f};
        };

        struct FindPicOptions
        {
            float threshold{90.0f};
            float angle_start{0.0f};
            float angle_extent{360.0f};
            float angle_step{1.0f};
            float scale_min{0.9f};
            float scale_max{1.1f};
            float scale_step{0.01f};
            float weak_thresh{30.0f};
            size_t num_features{64};
        };

        class TemplateMatcher
        {
        public:
            TemplateMatcher();
            TemplateMatcher(const TemplateMatcher& other);
            TemplateMatcher& operator=(const TemplateMatcher& other);
            TemplateMatcher(TemplateMatcher&& other) noexcept;
            TemplateMatcher& operator=(TemplateMatcher&& other) noexcept;
            ~TemplateMatcher();

            OperationResult createShapeModel(cv::InputArray image,
                                             cv::InputArray mask = cv::Mat(),
                                             const ShapeModelOptions& options = {});
            OperationResult createOneShapeModel(cv::InputArray image,
                                                cv::InputArray mask = cv::Mat(),
                                                const OneShapeModelOptions& options = {});

            Result<std::vector<Match>> findShapeModel(cv::InputArray image,
                                                      cv::InputArray mask = cv::Mat(),
                                                      const FindOptions& options = {}) const;
            Result<std::vector<Match>> findPicModel(cv::InputArray image,
                                                    cv::InputArray model,
                                                    cv::InputArray mask = cv::Mat(),
                                                    cv::InputArray model_mask = cv::Mat(),
                                                    const FindPicOptions& options = {});

            Result<Template> getTemplate(int id) const;
            Result<float> getAngle(int id) const;
            Result<float> getScale(int id) const;

            OperationResult writeShapeModel(const cv::String& fileName) const;
            OperationResult readShapeModel(const cv::String& fileName);

            bool empty() const;
            void clear();

        private:
            std::unique_ptr<ITemplMatch> impl_;
        };
    }
}

#endif  //BSCV_MODULES_TEMPLMATCH_INCLUDE_TEMPLMATCH_H
