#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <opencv2/objdetect/charuco_detector.hpp>
#include <opencv2/objdetect/aruco_dictionary.hpp>
#include <opencv2/objdetect/aruco_board.hpp>

// 相机标定核心类
class Calib {
public:
    // 构造函数
    Calib();
    // 析构函数
    ~Calib();

    // 运行标定函数
    // imageFiles: 输入的图片路径列表
    // boardSize: 棋盘格角点数量 (例如 cv::Size(9, 6) 表示内角点 9列 6行)
    // squareSize: 棋盘格每个方格的实际物理尺寸 (单位通常是毫米)
    // cameraMatrix: 输出的相机内参矩阵
    // distCoeffs: 输出的畸变系数
    // rvecs: 输出的旋转向量
    // tvecs: 输出的平移向量
    // imagePoints: 输出的所有图像检测到的角点
    // objectPoints: 输出的对应的物体空间点
    // 返回值: 重投影误差 (RMS error)，如果失败返回 -1.0
    double runCalibration(const std::vector<std::string>& imageFiles,const cv::Size boardSize,const float squareSize,
                          cv::Mat& cameraMatrix, cv::Mat& distCoeffs,
                          std::vector<cv::Mat>& rvecs, std::vector<cv::Mat>& tvecs,
                          std::vector<std::vector<cv::Point2f>>& imagePoints,
                          std::vector<std::vector<cv::Point3f>>& objectPoints);

    // ChArUco 标定函数
    // imageFiles: 输入的图片路径列表
    // boardSize: 棋盘格角点数量 (例如 cv::Size(9, 6) 表示内角点 9列 6行)
    // squareSize: 棋盘格每个方格的实际物理尺寸 (单位通常是毫米)
    // markerSize: ArUco 标记的大小 (单位通常是毫米)
    // cameraMatrix: 输出的相机内参矩阵
    // distCoeffs: 输出的畸变系数
    // rvecs: 输出的旋转向量
    // tvecs: 输出的平移向量

    // dictionaryId: 字典 ID，例如 cv::aruco::DICT_6X6_250
    double runCalibrationChArUco(const std::vector<std::string>& imageFiles, 
                          const cv::Size boardSize, 
                          const float squareSize, 
                          const float markerSize,
                          const int dictionaryId,
                          cv::Mat& cameraMatrix, cv::Mat& distCoeffs,
                          std::vector<cv::Mat>& rvecs, std::vector<cv::Mat>& tvecs,
                          std::vector<std::vector<cv::Point2f>>& imagePoints,
                          std::vector<std::vector<cv::Point3f>>& objectPoints);
};
