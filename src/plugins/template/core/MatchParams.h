#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <QVector>
#include <QVariant>
#include <QMetaType>

enum class MatchCenterType {
	SceneCenter,
	TrainCenter
};

struct MatchParams {
	double angleRange = 360.0;
	float angle_step = 1.0f;
	size_t FeaturePointNum = 128;
	int compressionLevel = 2;
	float weakThreshold = 30.0f;
	float strongThreshold = 60.0f;
	bool ignorePolarity = false;
	float scale_min = 0.95f;
	float scale_max = 1.05f;
	float scale_step = 0.01f;
	int borderPadding = 1;
};

struct FindMatchParams {
	int maxCount = 5;
	bool useSubPx = true;
	MatchCenterType centerType = MatchCenterType::SceneCenter;
	double scoreThreshold = 60.00;
	bool useRoi = false;//该参数必须设置false,目前未提供roi匹配
	cv::Mat Mask;
};

struct MatchResult {
	cv::Rect Roiarea;
	double score;
	double angle;
	double scale;
	cv::Point2f center;
	cv::RotatedRect rotatedRect;
	cv::Size2f featureSize{0.f, 0.f};
	std::vector<cv::Point2f> transformedFeaturePoints;
};

struct MatchResults {
	QVector<MatchResult> results;
};

Q_DECLARE_METATYPE(MatchResults);