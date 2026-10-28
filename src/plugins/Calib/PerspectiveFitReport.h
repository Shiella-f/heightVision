#pragma once
#include <opencv2/core.hpp>
#include <QString>
#include <string>

namespace calibration {
struct ResidualStatistics {
    double rms = 0, p95 = 0, maximum = 0;
};

// Residuals refer to the nominal board model, not independently measured accuracy.
struct PerspectiveFitReport {
    std::string inputStage;
    int pointCount = 0, inlierCount = 0;
    double toleranceCells = 0, squareMm = 0, pixPerMm = 0;
    ResidualStatistics all, inliers; // cell units

    void write(cv::FileStorage& fs) const {
        if(pointCount<=0)return;
        fs << "fit_report" << "{" << "input_stage" << inputStage
           << "residual_basis" << "nominal_board_geometry"
           << "tolerance_cells" << toleranceCells << "point_count" << pointCount
           << "inlier_count" << inlierCount << "inlier_ratio" << double(inlierCount)/pointCount;
        auto statistics = [&](const char* name, const ResidualStatistics& values) {
            fs << name << "{" << "rms_cells" << values.rms << "p95_cells" << values.p95 << "max_cells" << values.maximum
               << "rms_pixels" << values.rms*squareMm*pixPerMm << "p95_pixels" << values.p95*squareMm*pixPerMm
               << "max_pixels" << values.maximum*squareMm*pixPerMm
               << "rms_mm" << values.rms*squareMm << "p95_mm" << values.p95*squareMm << "max_mm" << values.maximum*squareMm << "}";
        };
        statistics("all_points",all); statistics("inlier_points",inliers); fs << "}";
    }

    QString description() const {
        QString message = QString("透视拟合：%1/%2 内点（%3%），容差 %4 格")
            .arg(inlierCount).arg(pointCount).arg(100.0*inlierCount/pointCount,0,'f',1).arg(toleranceCells,0,'f',4);
        auto append = [&](const QString& name, const ResidualStatistics& values) {
            message += QString("\n%1 RMS/P95/最大：%2/%3/%4 格；%5/%6/%7 输出像素；%8/%9/%10 mm")
                .arg(name).arg(values.rms,0,'f',6).arg(values.p95,0,'f',6).arg(values.maximum,0,'f',6)
                .arg(values.rms*squareMm*pixPerMm,0,'f',3).arg(values.p95*squareMm*pixPerMm,0,'f',3)
                .arg(values.maximum*squareMm*pixPerMm,0,'f',3).arg(values.rms*squareMm,0,'f',6)
                .arg(values.p95*squareMm,0,'f',6).arg(values.maximum*squareMm,0,'f',6);
        };
        append("全部点",all); append("内点",inliers);
        return message+"\n毫米残差基于棋盘名义尺寸，不等同实际测量精度。";
    }
};
}
