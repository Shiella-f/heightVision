#include "Calib.h"
#include <iostream>
#include <algorithm>

Calib::Calib() {
}

Calib::~Calib() {
}

double Calib::runCalibration(const std::vector<std::string>& imageFiles,const cv::Size boardSize,const float squareSize,
                             cv::Mat& cameraMatrix, cv::Mat& distCoeffs,
                             std::vector<cv::Mat>& rvecs, std::vector<cv::Mat>& tvecs,
                             std::vector<std::vector<cv::Point2f>>& imagePoints,
                             std::vector<std::vector<cv::Point3f>>& objectPoints)
{
    // 清空输出容器
    imagePoints.clear();
    objectPoints.clear();
    
    const bool debugVis = true;// 是否启用调试可视化
    std::vector<cv::Point3f> obj;
    for (int i = 0; i < boardSize.height; i++) {
        for (int j = 0; j < boardSize.width; j++) {
            obj.push_back(cv::Point3f(j * squareSize, i * squareSize, 0));
        }
    }
    cv::Size imageSize;
    for (const auto& file : imageFiles) {
        cv::Mat img = cv::imread(file);
        if (img.empty()) continue;
        if (imageSize.area() == 0) {
            imageSize = img.size();
        }
            cv::Mat gray;
            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
            // 存储当前图像检测到的角点
            std::vector<cv::Point2f> corners;
            // 优化的检测策略：先用带 FAST_CHECK 的快速检测，失败后逐步尝试更稳健的检测方法
            int flags_fast = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK;
            int flags_norm = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE;

            bool found = false;
            // 1) 第一轮：快速检测
            found = cv::findChessboardCorners(gray, boardSize, corners, flags_fast);
            // 如果第一轮失败，尝试去掉 FAST_CHECK 标志（更稳健但慢一些）
            if (!found) {
                std::cout << "[Calib] findChessboardCorners (without FAST_CHECK) for " << file << "\n";
                found = cv::findChessboardCorners(gray, boardSize, corners, flags_norm);
            }
            //如果仍然失败，尝试使用更稳健的 findChessboardCornersSB
            if (!found) {
                try {
                    found = cv::findChessboardCornersSB(gray, boardSize, corners, cv::CALIB_CB_NORMALIZE_IMAGE);
                    if (found) std::cout << "[Calib] findChessboardCornersSB succeeded for " << file << "\n";
                } catch (...) {
                }
            }
            // 如果未检测到，打印日志
            if (!found) {
                std::cout << "[Calib] Chessboard not found in " << file << " (size=" << gray.cols << "x" << gray.rows << ")\n";
                if (debugVis) {
                    cv::imshow("NotFound", gray);
                    cv::waitKey(0);
                }
            }

            if (found) {
            // 亚像素精确化，提高角点坐标精度
            // winSize: 搜索窗口大小 (11, 11)
            // zeroZone: 死区大小 (-1, -1) 表示没有死区
            // criteria: 终止条件 (最大迭代次数 30 或 误差小于 0.1)
            cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.1));
                if (debugVis) {
                    cv::Mat imgCorners = img.clone();
                    cv::drawChessboardCorners(imgCorners, boardSize, corners, found);
                    cv::namedWindow("Corners", cv::WINDOW_NORMAL);
                    cv::imshow("Corners", imgCorners);
                    cv::waitKey(0);
                }
            // 将检测到的 2D 角点加入列表
            imagePoints.push_back(corners);
            // 将对应的 3D 物体点加入列表 (每张图对应的 3D 点都是一样的)
            objectPoints.push_back(obj);
        }
    }

    // 如果有效图片数量太少(没有图片检测到角点)，无法标定
    if (imagePoints.empty()) {
        return -1.0;
    }

    // 开始相机标定
    // objectPoints: 世界坐标系中的点向量的向量
    // imagePoints: 图像坐标系中的点向量的向量
    // imageSize: 图像大小
    // cameraMatrix: 输出内参矩阵 (3x3)
    // distCoeffs: 输出畸变系数 (k1, k2, p1, p2, k3...)
    // rvecs, tvecs: 每张图片的旋转和平移向量
    double rms = cv::calibrateCamera(objectPoints, imagePoints, imageSize,
        cameraMatrix, distCoeffs, rvecs, tvecs);

    // 返回重投影误差
    return rms;
}

double Calib::runCalibrationChArUco(const std::vector<std::string>& imageFiles, 
                      const cv::Size boardSize, 
                      const float squareSize, 
                      const float markerSize,
                      const int dictionaryId,
                      cv::Mat& cameraMatrix, cv::Mat& distCoeffs,
                      std::vector<cv::Mat>& rvecs, std::vector<cv::Mat>& tvecs,
                      std::vector<std::vector<cv::Point2f>>& imagePoints,
                      std::vector<std::vector<cv::Point3f>>& objectPoints)
{
    // 存储所有图片的 Charuco 角点和 Object Points
    imagePoints.clear();
    objectPoints.clear();
    
    // 初始化 Dictionary 和 Board
    cv::aruco::Dictionary dictionary = cv::aruco::getPredefinedDictionary(dictionaryId);
    cv::aruco::CharucoBoard board(boardSize, squareSize, markerSize, dictionary);
    cv::aruco::CharucoDetector detector(board);
    
    // 获取所有的理论 3D 角点
    std::vector<cv::Point3f> allBoardCorners = board.getChessboardCorners();

    cv::Size imageSize;
    const bool debugVis = true; // 可视化开关

    for (const auto& file : imageFiles) {
        cv::Mat img = cv::imread(file);
        if (img.empty()) continue;
        if (imageSize.area() == 0) imageSize = img.size();

        std::vector<int> charucoIds;
        std::vector<cv::Point2f> charucoCorners;
        
        // 检测 Charuco 板
        // CharucoDetector::detectBoard 内部会检测 markers 并插值 charuco corners
        detector.detectBoard(img, charucoCorners, charucoIds);

        if (charucoIds.size() > 4) { // 至少需要一定数量的角点才能进行标定
            imagePoints.push_back(charucoCorners);
            
            // 收集对应的 3D 点
            std::vector<cv::Point3f> currentObjectPoints;
            currentObjectPoints.reserve(charucoIds.size());
            for(int id : charucoIds) {
                if(id >= 0 && id < (int)allBoardCorners.size())
                {
                    currentObjectPoints.push_back(allBoardCorners[id]);
                }
            }
            objectPoints.push_back(currentObjectPoints);
                
            if (debugVis) {
                cv::Mat imgCopy;
                img.copyTo(imgCopy);
                cv::aruco::drawDetectedCornersCharuco(imgCopy, charucoCorners, charucoIds);
                cv::imshow("Charuco", imgCopy);
                cv::waitKey(10); 
            }
        } else {
             std::cout << "[Calib] Not enough Charuco corners in " << file << "\n";
        }
    }
    
    if (debugVis) try { cv::destroyWindow("Charuco"); } catch(...){}

    // 如果有效数据太少，返回失败
    if (imagePoints.size() < 1) {
        return -1.0;
    }

    // 执行相机标定 (使用标准 calibrateCamera)
    double rms = cv::calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs);
    
    std::cout << "[Calib] ChArUco Calibration done. RMS: " << rms << "\n";
    return rms;
}

