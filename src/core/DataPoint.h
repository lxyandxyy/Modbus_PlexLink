#ifndef DATAPOINT_H
#define DATAPOINT_H

#include <QVariant>
#include <QDateTime>

namespace ModbusPlexLink {

// 数据质量枚举
enum class DataQuality : quint8 {
    Good = 0,       // 数据正常
    Bad = 1,        // 数据异常
    Uncertain = 2,  // 数据不确定
    NotUpdated = 3  // 数据未更新
};

// 数据点结构体
struct DataPoint {
    QVariant value;                                     // 数据值（支持多种类型）
    DataQuality quality = DataQuality::NotUpdated;     // 质量戳
    qint64 timestamp = 0;                              // UTC时间戳（毫秒）
    
    // 默认构造函数
    DataPoint() = default;
    
    // 带参构造函数
    DataPoint(const QVariant& val, DataQuality q = DataQuality::Good)
        : value(val), quality(q), timestamp(QDateTime::currentMSecsSinceEpoch()) {}
    
    // 判断数据是否有效
    bool isValid() const {
        return !value.isNull() && quality == DataQuality::Good;
    }
    
    // 判断数据是否过期（默认5秒）
    bool isExpired(qint64 maxAgeMs = 5000) const {
        return (QDateTime::currentMSecsSinceEpoch() - timestamp) > maxAgeMs;
    }
};

} // namespace ModbusPlexLink

#endif // DATAPOINT_H

