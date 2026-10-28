#pragma once
#include "BoardConfigurationPort.h"
#include "BoardDetection.h"
namespace calibration {
bool parseBoardConfiguration(const QJsonObject& config,BoardSpec& spec,QString& error,QJsonObject* normalized=nullptr);
QJsonObject observationReport(const BoardObservation& observation);
}
