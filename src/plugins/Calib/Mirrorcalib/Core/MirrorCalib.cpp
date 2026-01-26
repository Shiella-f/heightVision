#include "MirrorCalib.h"
#include <fstream>
#include <iostream>


static void filterGridPoints(const std::vector<cv::Point2f>& keypoints, cv::Size patternSize, std::vector<cv::Point2f>& outPoints) {
    if (keypoints.empty()) return;
    std::vector<cv::Point2f> originalPoints = keypoints;

    // 计算最小外接矩形以检测旋转角度
    cv::RotatedRect box = cv::minAreaRect(originalPoints);
    
    // 尝试两个角度：minAreaRect的角度，以及+90度
    // 这是为了应对 patternSize 非正方形时的方向对齐问题，以及解决倾斜导致的行划分失败
    float angles[2] = { box.angle, box.angle + 90.0f };

    struct IndexedPoint {
        cv::Point2f pt; // 旋转后的坐标
        int originalIdx;
    };

    for (float angle : angles) {
        // 1. 构建旋转矩阵，将点集旋转到大致水平
        cv::Mat rotMat = cv::getRotationMatrix2D(box.center, angle, 1.0);
        std::vector<cv::Point2f> rotatedPoints;
        cv::transform(originalPoints, rotatedPoints, rotMat);
        
        std::vector<IndexedPoint> points;
        for(size_t i=0; i<rotatedPoints.size(); ++i) {
            points.push_back({rotatedPoints[i], (int)i});
        }

        // 按 Y 坐标排序，区分行
        std::sort(points.begin(), points.end(), [](const IndexedPoint& a, const IndexedPoint& b) {
            return a.pt.y < b.pt.y;
        });

        // 根据包围盒高度估算行间距 (基于旋转后的坐标)
        cv::Rect bbox = cv::boundingRect(rotatedPoints);
        if (bbox.height == 0) continue;

        double expectedStride = (double)bbox.height / std::max(1, patternSize.height - 1);
        if (patternSize.height <= 1) expectedStride = bbox.height;

        double rowThreshold = expectedStride * 0.45; 
        rowThreshold = std::max(rowThreshold, 10.0);

        std::vector<std::vector<IndexedPoint>> rows;
        rows.push_back({points[0]});

        for (size_t i = 1; i < points.size(); ++i) {
            if (points[i].pt.y - rows.back().back().pt.y > rowThreshold) {
                rows.push_back({});
            }
            rows.back().push_back(points[i]);
        }

        // 过滤掉点数不足的行
        auto it = std::remove_if(rows.begin(), rows.end(), [&](const std::vector<IndexedPoint>& row){
            return row.size() < (size_t)patternSize.width;
        });
        rows.erase(it, rows.end());
        
        if (rows.size() < (size_t)patternSize.height) continue;

        // 如果行数过多，使用滑动窗口选择方差最小的连续行子集
        if (rows.size() > (size_t)patternSize.height) {
            double minRowVariance = DBL_MAX;
            std::vector<std::vector<IndexedPoint>> bestRows;

            for (size_t i = 0; i <= rows.size() - patternSize.height; ++i) {
                 std::vector<double> rowYMeans;
                 for (size_t k = 0; k < (size_t)patternSize.height; ++k) {
                     double sumY = 0;
                     for (const auto& p : rows[i+k]) sumY += p.pt.y;
                     rowYMeans.push_back(sumY / rows[i+k].size());
                 }

                 double sumDiff = 0, sumSqDiff = 0;
                 int n = 0;
                 for (size_t k = 0; k < rowYMeans.size() - 1; ++k) {
                     double diff = rowYMeans[k+1] - rowYMeans[k];
                     sumDiff += diff;
                     sumSqDiff += diff*diff;
                     n++;
                 }
                 double variance = (n > 0) ? (sumSqDiff/n - (sumDiff/n)*(sumDiff/n)) : 0.0;
                 
                 if (variance < minRowVariance) {
                     minRowVariance = variance;
                     bestRows.clear();
                     for (size_t k = 0; k < (size_t)patternSize.height; ++k) {
                         bestRows.push_back(rows[i+k]);
                     }
                 }
            }
            if (!bestRows.empty()) rows = bestRows;
        }

        // 针对每一行，按 X 排序并筛选列
        for (auto& row : rows) {
            std::sort(row.begin(), row.end(), [](const IndexedPoint& a, const IndexedPoint& b) {
                return a.pt.x < b.pt.x;
            });
            
            // 如果该行点数超过每行应有点数，通过间距一致性筛选
            if (row.size() > (size_t)patternSize.width) {
                double minCombinedVariance = DBL_MAX;
                std::vector<IndexedPoint> bestSubset;
                
                // 滑动窗口寻找方差最小的子集
                for (size_t i = 0; i <= row.size() - patternSize.width; ++i) {
                    double sumDx = 0, sumSqDx = 0;
                    int n = 0;
                    for (size_t j = 0; j < (size_t)patternSize.width - 1; ++j) {
                        double dx = row[i+j+1].pt.x - row[i+j].pt.x;
                        sumDx += dx;
                        sumSqDx += dx*dx;
                        n++;
                    }
                    if (n == 0) continue; 
                    double variance = (sumSqDx / n) - (sumDx / n)*(sumDx / n);
                    if (variance < minCombinedVariance) {
                        minCombinedVariance = variance;
                        bestSubset.clear();
                        for(size_t k=0; k<(size_t)patternSize.width; ++k) bestSubset.push_back(row[i+k]);
                    }
                }
                if(!bestSubset.empty()) row = bestSubset;
            }
        }

        // 检查结果是否完整
        bool ok = true;
        if (rows.size() != (size_t)patternSize.height) ok = false;
        else {
            for(const auto& row: rows) {
                if (row.size() != (size_t)patternSize.width) {
                    ok = false;
                    break;
                }
            }
        }

        // 如果找到合适的结果
        if (ok) {
            outPoints.clear();
            for(const auto& row : rows) {
                for(const auto& p : row) {
                    outPoints.push_back(originalPoints[p.originalIdx]);
                }
            }
            return;
        }
    }
    
    // 如果所有尝试都失败，清空结果
    outPoints.clear();
}


MirrorCalib::MirrorCalib() {}

MirrorCalib::~MirrorCalib() {}
// 寻找标记点
bool MirrorCalib::findMarkerPoints(const cv::Mat& image, cv::Size patternSize, std::vector<cv::Point2f>& centers) {
    
    if (image.empty()) return false;
    //实心圆点检测
    bool found = cv::findCirclesGrid(image, patternSize, centers, cv::CALIB_CB_SYMMETRIC_GRID);
    if (found) return true;

    //针对空心圆点检测（圆环转换为实心圆）
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    // 高斯模糊有助于平滑边缘，可能弥合微小缺口
    //cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);

    cv::Mat binary;
    cv::threshold(gray, binary, 150 ,255, cv::THRESH_BINARY);
    
    if(displayimg)
    {
    cv::Mat display1;
    display1 = binary.clone();
    cv::pyrDown(display1, display1);
    cv::pyrDown(display1, display1);
    cv::imshow("Detected Circles", display1);
    cv::waitKey(0);
    }

    // 使用较小的核进行闭运算，或者仅依赖凸包特性，避免过度膨胀影响其他区域
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);

    if(displayimg)
    {
    cv::Mat display2;
    display2 = binary.clone();
    cv::pyrDown(display2, display2);   
    cv::pyrDown(display2, display2); 
    cv::imshow("Detected Circles", display2);
    cv::waitKey(0);
    }

    std::vector<std::vector<cv::Point>> contours;
    // 改为 RETR_EXTERNAL 只检测外轮廓，避免圆环产生内外两个重复轮廓
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    std::vector<cv::Point2f> findcenters;
    // 创建纯净背景，准备绘制实心圆
    cv::Mat cleanImage = cv::Mat::zeros(gray.size(), CV_8UC1);

    for (const auto& contour : contours) {
        // 计算凸包：即使圆环有缺口，凸包通常也是完整的圆形
        std::vector<cv::Point> hull;
        cv::convexHull(contour, hull);

        // 使用凸包的面积和周长进行筛选
        double area = cv::contourArea(hull);
        if (area < 100 || area > 1000000) continue;

        double perimeter = cv::arcLength(hull, true);
        if (perimeter == 0) continue;
        double circularity = 4 * CV_PI * area / (perimeter * perimeter);

        if (circularity > 0.7) {
            cv::Point2f center;
            float radius;
            cv::minEnclosingCircle(hull, center, radius);

            // 简单去重：如果新点与已有点距离太近，则认为是重复点
            bool isDuplicate = false;
            for (const auto& existing : findcenters) {
                if (cv::norm(center - existing) < 60.0) {
                    isDuplicate = true;
                    break;
                }
            }
            
            if (!isDuplicate) {
                findcenters.push_back(center);
            }
        }
    }

    std::vector<cv::Point2f> filteredCenters;
    filterGridPoints(findcenters, patternSize, filteredCenters);
    for(const auto& pt : filteredCenters) {
        cv::circle(cleanImage, (cv::Point)pt, 5, cv::Scalar(255), -1);
    }
    if(displayimg)
    {
    cv::Mat display;
    display = cleanImage.clone();
    cv::pyrDown(display, display);
    cv::pyrDown(display, display);
    cv::imshow("Detected Circles", display);
    cv::waitKey(0);
    }
    
    // 配置 BlobDetector 参数
    cv::SimpleBlobDetector::Params params;
    params.filterByArea = true;
    params.minArea = 10;
    params.maxArea = 500; // 限制最大面积
    params.filterByCircularity = true;
    params.minCircularity = 0.7f;
    params.filterByConvexity = false; 
    params.filterByInertia = false;
    params.filterByColor = true;
    params.blobColor = 255;
    params.minDistBetweenBlobs = 5.0f;

    cv::Ptr<cv::FeatureDetector> blobDetector = cv::SimpleBlobDetector::create(params);
    found = cv::findCirclesGrid(cleanImage, patternSize, centers, cv::CALIB_CB_SYMMETRIC_GRID | cv::CALIB_CB_CLUSTERING, blobDetector);

    // 如果成功，使用高精度的 filteredCenters 替换 integer-based cleanImage detection results
    if (found && !filteredCenters.empty()) {
        for (auto& gridPt : centers) {
            int bestIdx = -1;
            double minDst = DBL_MAX;
            for (size_t i = 0; i < filteredCenters.size(); ++i) {
                double dst = cv::norm(gridPt - filteredCenters[i]);
                if (dst < minDst) {
                    minDst = dst;
                    bestIdx = (int)i;
                }
            }
            // 如果距离足够近（例如20像素内），则替换为亚像素坐标
            if (bestIdx != -1 && minDst < 20.0) {
                gridPt = filteredCenters[bestIdx];
            }
        }
    }

    // 如果没找到，尝试反转图像 (白底黑圆)
    if (!found) {
        cv::Mat invertedClean;
        cv::bitwise_not(cleanImage, invertedClean);
        params.blobColor = 0;
        blobDetector = cv::SimpleBlobDetector::create(params);
        found = cv::findCirclesGrid(invertedClean, patternSize, centers, cv::CALIB_CB_SYMMETRIC_GRID | cv::CALIB_CB_CLUSTERING, blobDetector);
    }
    
    if (!found) {
        cv::SimpleBlobDetector::Params params;
        params.filterByArea = true;
        params.minArea = 10;
        params.maxArea = 10000;
        // 创建检测器
        cv::Ptr<cv::SimpleBlobDetector> detector = cv::SimpleBlobDetector::create(params);
        std::vector<cv::KeyPoint> keypoints;
        detector->detect(image, keypoints);
        
        if (keypoints.size() == (size_t)(patternSize.width * patternSize.height)) {
            centers.clear();
            for (const auto& kp : keypoints) {
                centers.push_back(kp.pt);
            }
            found = true;
        }
    }
    
    return found;
}

bool MirrorCalib::saveComparisonToCSV(const QString& filepath, 
                                      const std::vector<cv::Point2f>& detected, 
                                      const std::vector<cv::Point2f>& preset) {
    std::ofstream file(filepath.toStdString());
    if (!file.is_open()) return false;
    file << "Index,PresetX,PresetY,DetectedX,DetectedY\n";
    
    size_t count = std::min(detected.size(), preset.size());
    
    for (size_t i = 0; i < count; ++i) {
        file << i << "," 
             << preset[i].x << "," << preset[i].y << ","
             << detected[i].x << "," << detected[i].y << "\n";
    }
    
    file.close();
    return true;
}

double MirrorCalib::calibrateHomography(const std::vector<cv::Point2f>& imagePoints, 
                                        const std::vector<cv::Point2f>& physicalPoints, 
                                        cv::Mat& outMatrix) {
    if ((imagePoints.size() == 9 && physicalPoints.size() == 9) || (imagePoints.size() == 81 && physicalPoints.size() == 81)) {
        outMatrix = cv::findHomography(imagePoints, physicalPoints);
        if (outMatrix.empty()) {
            return -1.0;
        }
    // 计算重投影误差
    std::vector<cv::Point2f> projectedPoints;
    cv::perspectiveTransform(imagePoints, projectedPoints, outMatrix);
    
    double totalError = 0.0;
    for (size_t i = 0; i < imagePoints.size(); ++i) {
        double error = cv::norm(projectedPoints[i] - physicalPoints[i]);
        totalError += error * error;
    }
    
    double rmsError = std::sqrt(totalError / imagePoints.size());
    return rmsError;
    } else {
        return -1.0;
    }
}   

bool MirrorCalib::saveCalibrationMatrix(const QString& filepath, const cv::Mat& matrix) {
    cv::FileStorage fs(filepath.toStdString(), cv::FileStorage::WRITE);
    if (!fs.isOpened()) return false;
    fs << "HomographyMatrix" << matrix;
    fs.release();
    return true;
}
