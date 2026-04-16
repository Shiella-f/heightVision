#ifndef ORIONVISIONGLOBAL_H
#define ORIONVISIONGLOBAL_H

#include "QPointF"
#include "QRectF"
#include "QSizeF"
#include <QVariant>
#include <QMetaType>
#include <QVector>
#include <QDebug>
#include <QPainterPath>
#include "MatchParams.h"

struct CopyItems_Move_Rotate_Data {
    QVector<QPointF> moveOffsets;
    QVector<double> angles;
    int totalCounts;
    QVector<MatchResult> matchResultVec;

    CopyItems_Move_Rotate_Data() : totalCounts(0) {}

    CopyItems_Move_Rotate_Data(const QPointF& offset, double rotAngle) {
        moveOffsets.append(offset);
        angles.append(rotAngle);
        totalCounts = 1;
    }

    CopyItems_Move_Rotate_Data(const QVector<QPointF>& offsets, const QVector<double>& rotAngles)
        : moveOffsets(offsets), angles(rotAngles) {
        if (moveOffsets.size() != angles.size()) {
            qWarning() << "警告：偏移量数组和角度数组长度不一致！";
            totalCounts = qMin(moveOffsets.size(), angles.size());
        } else {
            totalCounts = moveOffsets.size();
        }
    }

    void addData(const QPointF& offset, double rotAngle) {
        moveOffsets.append(offset);
        angles.append(rotAngle);
        totalCounts = moveOffsets.size();
    }

    void clear() {
        moveOffsets.clear();
        angles.clear();
        totalCounts = 0;
    }

    int count() const {
        int actualCount = qMin(moveOffsets.size(), angles.size());
        return (totalCounts == actualCount) ? totalCounts : actualCount;
    }

    void setTotalCounts(unsigned int counts) {
        if (counts > moveOffsets.size() || counts > angles.size()) {
            qWarning() << "警告：设置的总数超过实际数据组数！";
        }
        totalCounts = counts;
    }
};
Q_DECLARE_METATYPE(CopyItems_Move_Rotate_Data);

struct initOrionVisionParam {
    QRectF unitedRect;
    QSizeF canvasSize;
    QPainterPath combinedPath;
    QImage img;
};
Q_DECLARE_METATYPE(initOrionVisionParam);

#endif // ORIONVISIONGLOBAL_H
