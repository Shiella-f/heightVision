#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <optional>
#include <QString>

namespace Height::core {

class HeightCore {
public:
    HeightCore() = default;
    ~HeightCore() = default;

    // 表示单张图像及其识别结果
    struct ImageInfo {
        std::string path;               // 文件路径
        cv::Mat image;                   // 原始图像（按需加载）
        bool spot1Found = false;        // 光斑1 是否找到
        bool spot2Found = false;        // 光斑2 是否找到
        cv::Point2f spot1Pos;           // 光斑1 像素坐标
        cv::Point2f spot2Pos;           // 光斑2 像素坐标
        float spot1Radius = 0.0f;       // 光斑1 半径
        float spot2Radius = 0.0f;       // 光斑2 半径
        std::optional<double> distancePx; // 两光斑像素距离
        std::optional<double> distanceMm; // 两光斑毫米距离（不需要）
        std::optional<double> heightMm;   // 最终计算得到的高度（mm）
    };

    bool loadFolder(const QString& folderPath);
    // 获取当前所有图像信息（包括识别结果和高度）
    const std::vector<ImageInfo>& getImageInfos() const;
    // 获取当前所有图像对应的高度间隔（毫米），默认 2.0 mm
    double getStepMm() const;
    // 获取列表中所有图片的像素距离，返回对应的距离列表
    std::vector<double> getDistancePxList() const;
    //计算列表中所有图片的高度值（mm）
    bool computeHeightValues();
    // 返回列表中所有图片的高度列表
    std::vector<double> getHeightList() const;
    // 导入为指定高度的参考图,返回 true 表示成功找到并设置。
    bool importImageAsReference(const QString& imagePath);
    // 设置/获取当前参考高度（mm）
    bool setPreferenceHeight(double heightMm);
    double getPreferenceHeight() const;
    // 线性标定：height_mm = calibA * distance_px + calibB
    bool setCalibrationLinear();
    //获取线性标定参数
    void getCalibrationLinear(double& outA, double& outB) const;
    void setIsLinearCalib(bool isLinear){ m_hasLinearCalib = isLinear; }
    bool hasLinearCalib() const { return m_hasLinearCalib; }
    // 加载测试图片信息
    bool loadTestImageInfo(const QString& imagePath);
    bool loadTestImageInfo(const cv::Mat& image);
    //计算待测图片信息
    bool computeTestImageInfo();
    //执行测高，并返回测量值（mm），若该图片未识别光斑或无法计算则返回 empty
    std::optional<double> measureHeightForImage() const;
    cv::Mat getShowImage() const{return m_showImage;}
    // 设置光斑识别区域（以图像坐标系的矩形表示），后续识别会在该 ROI 内进行
    void setROI(const cv::Rect2f& roi);
    //设置阈值
    void setThreshold(int thresh){ threshold = thresh; }
    void setProcessedIsDisplay(bool isDisplay){ processedIsDisplay = isDisplay; }
    //比例尺设置（预留）
    void setPixelToMm(double pxToMm);
    double getPixelToMm() const;
    //保存线性标定参数
    bool saveCalibrationData(const QString& filePath) const;
    //加载线性标定参数
    bool loadCalibrationData(const QString& filePath);

private:
    // 对已加载的图片进行双光斑识别，会将识别结果写回 ImageInfo
    bool detectTwoSpotsInImage();
    // 计算两点的像素距离
    static double computeDistancePx(const cv::Point2f& a, const cv::Point2f& b);
    // 设置相邻两张图像对应的高度间隔（毫米），默认 2.0 mm
    void setStepMm(double stepMm);
    // 按文件名或载入顺序为图像排序
    void sortImagesByName();
    // 根据 distancePx 对图像进行排序
    void rankImagesByDistancePx();
    // 处理单张图像，检测光斑位置及半径
    bool processImage(const cv::Mat& input,
                      std::vector<std::pair<cv::Point2f, float>>& detectedSpots) const;
    // 从候选圆中挑选面积差异可接受的圆心集合
    bool selectBalancedPair(const std::vector<std::pair<cv::Point2f, float>>& spots,
                            double maxAreaRatio,
                            std::vector<std::pair<cv::Point2f, float>>& outSpots) const;

private:
    cv::Mat m_currentImage;      // 当前处理的图像
    std::vector<ImageInfo> m_images;
    cv::Mat m_showImage;    // 用于显示的图像
    ImageInfo m_testImageInfo; // 测量图像信息
    cv::Rect m_roi = cv::Rect(); // 光斑识别区域
    double m_stepMm = 2.0;         // 默认采样间隔（mm）
    
    // 标定相关
    double m_pxToMm = 0.0;         // 像素->毫米比例；若为 0 则表示未设置
    bool m_hasLinearCalib = false; // 是否设置了线性标定
    double m_calibA = 0.0;         // 线性系数 a
    double m_calibB = 0.0;         // 线性偏置 b

    double m_preferenceHeight = 0.0;  // 当前设置的高度参考（mm）
    cv::Mat m_preferenceImage;    // 参考图像
    int m_referenceImgId = -1; // 当前参考图像Id
    mutable std::vector<cv::Point2f> m_lastDetectedCenters; // 最近一次检测出的圆心坐标集合

    int threshold = 180;
    bool processedIsDisplay = false;

    double minArea = 1;   // 最小面积
    double maxArea = 10000; // 最大面积
    double kMaxAreaRatio = 4; // 控制两个圆面积的最大允许比值；

    int Usechannel = 1; // 使用的通道，默认使用红色通道
    };

} // namespace Height::core
