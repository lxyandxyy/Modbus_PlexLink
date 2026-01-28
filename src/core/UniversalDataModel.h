#ifndef UNIVERSALDATAMODEL_H
#define UNIVERSALDATAMODEL_H

#include <QObject>
#include <QHash>
#include <QReadWriteLock>
#include <QStringList>
#include <QTimer>
#include "DataPoint.h"

namespace ModbusPlexLink {

// UDM更新信号参数
struct DataUpdateInfo {
    QString tagName;
    DataPoint oldValue;
    DataPoint newValue;
};

class UniversalDataModel : public QObject {
    Q_OBJECT
    
public:
    explicit UniversalDataModel(QObject *parent = nullptr);
    ~UniversalDataModel();
    
    // 写入/更新一个数据点
    void updatePoint(const QString& tagName, const DataPoint& point);
    
    // 批量更新数据点
    void updatePoints(const QHash<QString, DataPoint>& points);
    
    // 读取一个数据点
    DataPoint readPoint(const QString& tagName) const;
    
    // 批量读取数据点
    QList<DataPoint> readPoints(const QStringList& tagNames) const;
    
    // 读取所有数据点
    QHash<QString, DataPoint> readAllPoints() const;
    
    // 检查标签是否存在
    bool hasTag(const QString& tagName) const;
    
    // 获取所有标签名
    QStringList getAllTags() const;
    
    // 清除指定标签
    void removeTag(const QString& tagName);
    
    // 清除所有数据
    void clear();
    
    // 获取数据点数量
    int size() const;
    
    // 设置数据过期检查间隔（毫秒）
    void setExpirationCheckInterval(int intervalMs);
    
    // 设置数据最大年龄（毫秒）
    void setMaxDataAge(qint64 maxAgeMs);
    
signals:
    // 数据更新信号
    void dataUpdated(const QString& tagName, const DataPoint& point);
    
    // 批量数据更新信号
    void batchDataUpdated(const QList<DataUpdateInfo>& updates);
    
    // 数据过期信号
    void dataExpired(const QString& tagName);
    
    // 标签添加信号
    void tagAdded(const QString& tagName);
    
    // 标签移除信号
    void tagRemoved(const QString& tagName);
    
private slots:
    // 检查数据过期
    void checkDataExpiration();
    
private:
    // 数据缓存
    QHash<QString, DataPoint> m_dataCache;
    
    // 读写锁（保证线程安全）
    mutable QReadWriteLock m_lock;
    
    // 过期检查定时器
    QTimer* m_expirationTimer;
    
    // 数据最大年龄（毫秒）
    qint64 m_maxDataAge;
    
    // 是否启用过期检查
    bool m_enableExpirationCheck;
};

} // namespace ModbusPlexLink

#endif // UNIVERSALDATAMODEL_H

