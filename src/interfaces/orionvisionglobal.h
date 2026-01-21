#ifndef ORIONVISIONGLOBAL_H
#define ORIONVISIONGLOBAL_H

#include "QPointF"
#include "QRectF"
#include "QSizeF"
#include <QVariant>
#include <QMetaType>
#include <QVector>
#include <QDebug>

// 定义数据复制粘贴的移动与旋转数据
struct CopyItems_Move_Rotate_Data {
    QVector<QPointF> moveOffsets;  // 多组移动偏移量（动态数组）
    QVector<double> angles;        // 多组旋转角度（动态数组）
    int totalCounts;      // 总数据组数（与数组长度关联）
    QVector<MatchResult> matchResultVec;  // 多组匹配结果数据（动态数组）

    // 1. 默认构造函数：初始化totalCounts为0
    CopyItems_Move_Rotate_Data() : totalCounts(0) {}
    // 2. 构造函数：初始化单组数据（兼容原有逻辑）
    CopyItems_Move_Rotate_Data(const QPointF& offset, double rotAngle) {
        moveOffsets.append(offset);
        angles.append(rotAngle);
        totalCounts = 1; // 单组数据，总数为1
    }
    // 3. 构造函数：直接传入数组（批量初始化）
    CopyItems_Move_Rotate_Data(const QVector<QPointF>& offsets, const QVector<double>& rotAngles)
        : moveOffsets(offsets), angles(rotAngles) {
        // 校验两个数组长度一致（避免数据不匹配）
        if (moveOffsets.size() != angles.size()) {
            qWarning() << "警告：偏移量数组和角度数组长度不一致！";
            totalCounts = qMin(moveOffsets.size(), angles.size()); // 取较小值作为有效总数
        } else {
            totalCounts = moveOffsets.size(); // 数组长度一致时，总数等于数组长度
        }
    }
    // 4. 便捷方法：添加一组数据（同步更新总数）
    void addData(const QPointF& offset, double rotAngle) {
        moveOffsets.append(offset);
        angles.append(rotAngle);
        totalCounts = moveOffsets.size(); // 添加后更新总数
    }
    // 5. 便捷方法：清空所有数据（同步重置总数）
    void clear() {
        moveOffsets.clear();
        angles.clear();
        totalCounts = 0;
    }
    // 6. 便捷方法：获取有效数据组数（兼容totalCounts，保证准确性）
    int count() const {
        // 优先返回实际有效数组长度，避免totalCounts与数组不一致
        int actualCount = qMin(moveOffsets.size(), angles.size());
        return (totalCounts == actualCount) ? totalCounts : actualCount;
    }
    // 7. 便捷方法：手动设置总数（如需强制指定，慎用）
    void setTotalCounts(unsigned int counts) {
        if (counts > moveOffsets.size() || counts > angles.size()) {
            qWarning() << "警告：设置的总数超过实际数据组数！";
        }
        totalCounts = counts;
    }
};
Q_DECLARE_METATYPE(CopyItems_Move_Rotate_Data);
// 初次打开视觉插件的参数结构体
struct initOrionVisionParam {       // 默认单位pixels
    QRectF unitedRect;              
    QSizeF canvasSize;
    QPainterPath combinedPath;
    QImage img;
    
};
Q_DECLARE_METATYPE(initOrionVisionParam);

#endif // ORIONVISIONGLOBAL_H


