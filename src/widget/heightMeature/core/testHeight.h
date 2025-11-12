#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

#include <string>
#include <optional>

using namespace cv;
using namespace std;

namespace Height::core {

// 场景：有两个激光发射器，对应图像上两个光斑；当被测物体在某一高度时两光斑重合。
// 通过两光斑间距（像素或毫米）计算高度。头文件仅声明接口，具体算法在 .cpp 中实现。
class HeightCore {
public:
    HeightCore() = default;
    ~HeightCore() = default;

    // 表示单张图像及其识别结果（支持两个光斑）
    struct ImageInfo {
        std::string path;               // 文件路径
        cv::Mat image;                   // 原始图像（按需加载）

        bool spot1Found = false;        // 光斑1 是否找到
        bool spot2Found = false;        // 光斑2 是否找到
        cv::Point2f spot1Pos;           // 光斑1 像素坐标
        cv::Point2f spot2Pos;           // 光斑2 像素坐标

        std::optional<double> distancePx; // 两光斑像素距离
        std::optional<double> distanceMm; // 两光斑毫米距离（不需要）
        std::optional<double> heightMm;   // 最终计算得到的高度（mm）
    };

    // 载入文件夹，查找图片文件并建立 ImageInfo 列表（不一定加载全部像素数据）
    bool loadFolder();

    // 清空已加载数据
    void clear();

    // 获取当前所有图像信息（包括识别结果和高度）
    const std::vector<ImageInfo>& getImageInfos() const;

    // 对已加载的图片进行双光斑识别，会将识别结果写回 ImageInfo
    bool detectTwoSpotsInImage();

    // 计算两点的像素距离
    static double computeDistancePx(const cv::Point2f& a, const cv::Point2f& b);

    // 设置相邻两张图像对应的高度间隔（毫米），默认 2.0 mm
    void setStepMm(double stepMm);
    double getStepMm() const;
    
    // 获取列表中所有图片的像素距离，返回对应的距离列表
    std::vector<double> getDistancePxList() const;
    
    //计算列表中所有图片的高度值（mm）
    bool computeHeightValues();
    // 返回列表中所有图片的高度列表
    std::vector<double> getHeightList() const;

    // 导入为指定高度的参考图,返回 true 表示成功找到并设置。
    bool importImageAsReference();
    // 设置/获取当前参考高度（mm）
    bool setPreferenceHeight(double heightMm);
    double getPreferenceHeight() const;

    // 线性标定：height_mm = calibA * distance_px + calibB
    bool setCalibrationLinear();
    //获取线性标定参数
    void getCalibrationLinear(double& outA, double& outB) const;
    void setIsLinearCalib(bool isLinear){ m_hasLinearCalib = isLinear; }

    // 加载测试图片信息
    bool loadTestImageInfo();
    //计算待测图片信息
    bool computeTestImageInfo();
    //执行测高，并返回测量值（mm），若该图片未识别光斑或无法计算则返回 empty
    std::optional<double> measureHeightForImage() const;

    //ROI
    // 设置光斑识别区域（以图像坐标系的矩形表示），后续识别会在该 ROI 内进行
    void setROI(const cv::Rect& roi);
    cv::Rect getROI() const;

    //比例尺设置（预留）
    void setPixelToMm(double pxToMm);
    double getPixelToMm() const;

private:
    std::vector<ImageInfo> m_images;
    ImageInfo m_testImageInfo; // 测量图像信息
    cv::Rect m_roi{0,0,0,0};
    double m_stepMm = 2.0;         // 默认采样间隔（mm）
    

    // 标定相关
    double m_pxToMm = 0.0;         // 像素->毫米比例；若为 0 则表示未设置
    bool m_hasLinearCalib = false; // 是否设置了线性标定
    double m_calibA = 0.0;         // 线性系数 a
    double m_calibB = 0.0;         // 线性偏置 b

    cv::Mat m_currentImage;      // 当前处理的图像

    double m_preferenceHeight = 0.0;  // 当前设置的高度参考（mm）
    cv::Mat m_preferenceImage;    // 参考图像
    int m_referenceImgId = -1; // 当前参考图像Id
    mutable std::vector<cv::Point2f> m_lastDetectedCenters; // 最近一次检测出的圆心坐标集合

    // 按文件名或载入顺序为图像排序
    void sortImagesByName();
    // 根据 distancePx 对图像进行排序
    void rankImagesByDistancePx();
    bool processImage(const cv::Mat& input,
                      std::vector<std::pair<cv::Point2f, float>>& detectedSpots,
                      cv::Mat* debugOutput = nullptr) const;
};

} // namespace Height::core
