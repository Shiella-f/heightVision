#pragma once
#include <QJsonObject>
#include <QString>
#include <opencv2/core.hpp>
#if defined(CalibPLUGIN_LIBRARY)
#define BOARD_CONFIG_EXPORT Q_DECL_EXPORT
#else
#define BOARD_CONFIG_EXPORT Q_DECL_IMPORT
#endif
namespace calibration {
BOARD_CONFIG_EXPORT QJsonObject DefaultBoardConfiguration();
BOARD_CONFIG_EXPORT bool NormalizeBoardConfiguration(const QJsonObject& input,QJsonObject& result,QString& error);
BOARD_CONFIG_EXPORT bool PreviewBoardConfiguration(const QJsonObject& config,cv::Mat& image,QString& error);
BOARD_CONFIG_EXPORT bool InspectBoardConfiguration(const cv::Mat& image,const QJsonObject& config,QJsonObject& report);
}
