#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <stdexcept>

//相机内参 + 畸变系数
struct IntrinsicParams {
    cv::Mat cameraMatrix;   ///< 3×3 内参矩阵 K
    cv::Mat distCoeffs;     ///< 畸变系数 (k1,k2,p1,p2[,k3,...])
    cv::Size imageSize;     ///< 标定时使用的图像尺寸
};

//倾斜校正参数
struct TiltParams {
    cv::Mat homography;     ///< 3×3 透视变换矩阵 H
    cv::Size outputSize;    ///< 校正后输出图像尺寸
};

//校正结果元信息
struct CorrectionInfo {
    bool distortionApplied = false;
    bool tiltApplied       = false;
    double reprojError     = -1.0;  ///< 畸变重投影误差（若执行了标定）
};


class CameraCorrector {
public:

    CameraCorrector() = default;
    ~CameraCorrector() = default;

    // 禁止拷贝，允许移动
    CameraCorrector(const CameraCorrector&) = delete;
    CameraCorrector& operator=(const CameraCorrector&) = delete;
    CameraCorrector(CameraCorrector&&) = default;
    CameraCorrector& operator=(CameraCorrector&&) = default;


    /**
     * @brief 直接设置已有的内参（你自己标定完的结果）
     * @param cameraMatrix  3×3 内参矩阵
     * @param distCoeffs    畸变系数向量
     * @param imageSize     对应图像尺寸
     */
    void setIntrinsics(const cv::Mat& cameraMatrix,
                       const cv::Mat& distCoeffs,
                       const cv::Size& imageSize);

    /**
     * @brief 从 OpenCV XML/YAML 文件加载内参
     * @param filePath  文件路径，需包含 "cameraMatrix" 和 "distCoeffs" 节点
     */
    void loadIntrinsics(const std::string& filePath);

    /**
     * @brief 保存内参到 XML/YAML 文件
     */
    void saveIntrinsics(const std::string& filePath) const;

    /**
     * @brief 使用标定板图像序列重新标定（可选，若你已有内参可跳过）
     *
     * @param images        标定图像列表
     * @param boardSize     棋盘格内角点数，如 cv::Size(9,6)
     * @param squareSize    方格物理尺寸（米或毫米，单位统一即可）
     * @return              重投影误差 RMS
     */
    double calibrateFromImages(const std::vector<cv::Mat>& images,
                               const cv::Size& boardSize,
                               float squareSize);

    /**
     * @brief 预计算畸变矫正的重映射表（提升实时性能）
     * 调用 correctDistortion() 前建议先调用此函数
     */
    void initUndistortMaps();

    /**
     * @brief 对单帧图像执行畸变校正
     * @param src   输入原始图像
     * @param dst   输出去畸变图像
     * @param crop  是否裁剪掉黑边（使用 alpha=0 的最大内接矩形）
     */
    void correctDistortion(const cv::Mat& src,
                           cv::Mat& dst,
                           bool crop = false);


    // ===============================================================
    //  Stage 2: 倾斜（透视）校正相关接口
    // ===============================================================

    /**
     * @brief 手动指定4点对来计算单应矩阵（最灵活的方式）
     *
     * 典型用法：
     *   - 在去畸变后的图像上，目视选取一个已知为矩形的物体的4个角点
     *   - 提供这4个角点在"理想正视图"中应对应的4个位置
     *
     * @param srcPoints   原图中4个角点（顺序：左上、右上、右下、左下）
     * @param dstPoints   目标图中对应的4个点
     * @param outputSize  输出图像尺寸
     */
    void setTiltCorrectionByPoints(const std::vector<cv::Point2f>& srcPoints,
                                   const std::vector<cv::Point2f>& dstPoints,
                                   const cv::Size& outputSize);

    /**
     * @brief 自动倾斜校正：在图像中检测最大矩形轮廓，将其展平为矩形
     *
     * 适用场景：图像中有清晰的矩形目标（如标定板、文件、屏幕）。
     *
     * @param referenceImage  参考帧（建议使用去畸变后的图像）
     * @param outputSize      输出图像尺寸（若为 Size(0,0) 则自动推算）
     * @param debugView       若不为空，会在此图上绘制检测到的轮廓
     * @return                是否成功检测到矩形
     */
    bool autoDetectTilt(const cv::Mat& referenceImage,
                        const cv::Size& outputSize = cv::Size(0, 0),
                        cv::Mat* debugView = nullptr);

    /**
     * @brief 通过图像中的水平/垂直参考线估计倾斜角度并校正
     *
     * 适用场景：场景中有明确的水平或竖直线条（如地面线、门框）。
     *
     * @param referenceImage  参考帧
     * @param isHorizontal    true=以水平线为基准，false=以竖直线为基准
     */
    void estimateTiltFromLines(const cv::Mat& referenceImage,
                               bool isHorizontal = true);

    /**
     * @brief 直接设置已知的单应矩阵（来自外部标定或上次保存）
     */
    void setHomography(const cv::Mat& H, const cv::Size& outputSize);

    /**
     * @brief 保存倾斜校正参数
     */
    void saveTiltParams(const std::string& filePath) const;

    /**
     * @brief 从文件加载倾斜校正参数
     */
    void loadTiltParams(const std::string& filePath);

    /**
     * @brief 对单帧图像执行倾斜校正（透视变换）
     * @param src  输入图像（建议先经过畸变校正）
     * @param dst  输出图像
     */
    void correctTilt(const cv::Mat& src, cv::Mat& dst);


    // ===============================================================
    //  全流程一键接口
    // ===============================================================

    /**
     * @brief 两阶段全流程校正：畸变校正 → 倾斜校正
     * @param src   原始输入帧
     * @param dst   最终校正后图像
     * @param info  可选，返回本次校正的元信息
     */
    void correct(const cv::Mat& src,
                 cv::Mat& dst,
                 CorrectionInfo* info = nullptr);


    // ===============================================================
    //  状态查询
    // ===============================================================

    bool hasIntrinsics()  const { return intrinsicsReady_; }
    bool hasTiltParams()  const { return tiltReady_;       }
    bool hasMaps()        const { return mapsReady_;       }

    const IntrinsicParams& getIntrinsics() const { return intrinsic_; }
    const TiltParams&      getTiltParams() const { return tilt_;       }


private:
    // -------------------------------------------------------
    //  内部工具方法
    // -------------------------------------------------------

    /** 对4个点按顺序排列（左上→右上→右下→左下） */
    static std::vector<cv::Point2f> sortCorners(
        const std::vector<cv::Point2f>& pts);

    /** 从轮廓中找到最大四边形 */
    static bool findLargestQuad(const cv::Mat& gray,
                                std::vector<cv::Point2f>& corners,
                                cv::Mat* debugView);

    /** 用霍夫变换检测主方向线并估计倾斜角 */
    static double estimateDominantAngle(const cv::Mat& gray,
                                        bool isHorizontal);

    /** 根据倾斜角生成旋转+平移仿射矩阵并封装为单应 */
    static cv::Mat angleToHomography(double angleDeg,
                                     const cv::Size& imageSize,
                                     cv::Size& outputSize);

    // -------------------------------------------------------
    //  数据成员
    // -------------------------------------------------------
    IntrinsicParams intrinsic_;
    TiltParams      tilt_;

    // 畸变矫正重映射表（initUndistortMaps 后有效）
    cv::Mat map1_, map2_;

    bool intrinsicsReady_ = false;
    bool tiltReady_       = false;
    bool mapsReady_       = false;
};