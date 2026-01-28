#include "UniversalDataModel.h"
#include <QDebug>

namespace ModbusPlexLink {

UniversalDataModel::UniversalDataModel(QObject *parent)
    : QObject(parent)
    , m_maxDataAge(30000) // 默认30秒
    , m_enableExpirationCheck(true)
{
    // 初始化过期检查定时器
    m_expirationTimer = new QTimer(this);
    m_expirationTimer->setInterval(5000); // 默认5秒检查一次
    connect(m_expirationTimer, &QTimer::timeout, this, &UniversalDataModel::checkDataExpiration);
    
    if (m_enableExpirationCheck) {
        m_expirationTimer->start();
    }
}

UniversalDataModel::~UniversalDataModel() {
    m_expirationTimer->stop();
}

void UniversalDataModel::updatePoint(const QString& tagName, const DataPoint& point) {
    if (tagName.isEmpty()) {
        qWarning() << "UniversalDataModel::updatePoint: Empty tag name provided";
        return;
    }
    
    DataPoint oldValue;
    bool isNewTag = false;
    
    {
        QWriteLocker locker(&m_lock);
        
        if (m_dataCache.contains(tagName)) {
            oldValue = m_dataCache[tagName];
        } else {
            isNewTag = true;
        }
        
        m_dataCache[tagName] = point;
    }
    
    // 发送信号（在锁外部，避免死锁）
    if (isNewTag) {
        emit tagAdded(tagName);
    }
    
    emit dataUpdated(tagName, point);
}

void UniversalDataModel::updatePoints(const QHash<QString, DataPoint>& points) {
    if (points.isEmpty()) {
        return;
    }
    
    QList<DataUpdateInfo> updates;
    QStringList newTags;
    
    {
        QWriteLocker locker(&m_lock);
        
        for (auto it = points.constBegin(); it != points.constEnd(); ++it) {
            const QString& tagName = it.key();
            const DataPoint& newPoint = it.value();
            
            if (tagName.isEmpty()) {
                qWarning() << "UniversalDataModel::updatePoints: Empty tag name in batch update";
                continue;
            }
            
            DataUpdateInfo info;
            info.tagName = tagName;
            info.newValue = newPoint;
            
            if (m_dataCache.contains(tagName)) {
                info.oldValue = m_dataCache[tagName];
            } else {
                newTags.append(tagName);
            }
            
            m_dataCache[tagName] = newPoint;
            updates.append(info);
        }
    }
    
    // 发送信号（在锁外部）
    for (const QString& tag : newTags) {
        emit tagAdded(tag);
    }
    
    if (!updates.isEmpty()) {
        emit batchDataUpdated(updates);
    }
}

DataPoint UniversalDataModel::readPoint(const QString& tagName) const {
    if (tagName.isEmpty()) {
        qWarning() << "UniversalDataModel::readPoint: Empty tag name provided";
        return DataPoint();
    }
    
    QReadLocker locker(&m_lock);
    return m_dataCache.value(tagName, DataPoint());
}

QList<DataPoint> UniversalDataModel::readPoints(const QStringList& tagNames) const {
    QList<DataPoint> results;
    
    if (tagNames.isEmpty()) {
        return results;
    }
    
    QReadLocker locker(&m_lock);
    
    for (const QString& tagName : tagNames) {
        if (tagName.isEmpty()) {
            qWarning() << "UniversalDataModel::readPoints: Empty tag name in list";
            results.append(DataPoint());
            continue;
        }
        results.append(m_dataCache.value(tagName, DataPoint()));
    }
    
    return results;
}

QHash<QString, DataPoint> UniversalDataModel::readAllPoints() const {
    QReadLocker locker(&m_lock);
    return m_dataCache;
}

bool UniversalDataModel::hasTag(const QString& tagName) const {
    if (tagName.isEmpty()) {
        return false;
    }
    
    QReadLocker locker(&m_lock);
    return m_dataCache.contains(tagName);
}

QStringList UniversalDataModel::getAllTags() const {
    QReadLocker locker(&m_lock);
    return m_dataCache.keys();
}

void UniversalDataModel::removeTag(const QString& tagName) {
    if (tagName.isEmpty()) {
        qWarning() << "UniversalDataModel::removeTag: Empty tag name provided";
        return;
    }
    
    bool removed = false;
    
    {
        QWriteLocker locker(&m_lock);
        removed = m_dataCache.remove(tagName) > 0;
    }
    
    if (removed) {
        emit tagRemoved(tagName);
    }
}

void UniversalDataModel::clear() {
    QStringList removedTags;
    
    {
        QWriteLocker locker(&m_lock);
        removedTags = m_dataCache.keys();
        m_dataCache.clear();
    }
    
    for (const QString& tag : removedTags) {
        emit tagRemoved(tag);
    }
}

int UniversalDataModel::size() const {
    QReadLocker locker(&m_lock);
    return m_dataCache.size();
}

void UniversalDataModel::setExpirationCheckInterval(int intervalMs) {
    if (intervalMs <= 0) {
        m_expirationTimer->stop();
        m_enableExpirationCheck = false;
    } else {
        m_expirationTimer->setInterval(intervalMs);
        if (!m_expirationTimer->isActive() && m_enableExpirationCheck) {
            m_expirationTimer->start();
        }
    }
}

void UniversalDataModel::setMaxDataAge(qint64 maxAgeMs) {
    m_maxDataAge = maxAgeMs;
}

void UniversalDataModel::checkDataExpiration() {
    if (!m_enableExpirationCheck || m_maxDataAge <= 0) {
        return;
    }
    
    QStringList expiredTags;
    
    {
        QReadLocker locker(&m_lock);
        
        for (auto it = m_dataCache.constBegin(); it != m_dataCache.constEnd(); ++it) {
            const QString& tagName = it.key();
            const DataPoint& point = it.value();
            
            if (point.isExpired(m_maxDataAge)) {
                expiredTags.append(tagName);
            }
        }
    }
    
    // 发送过期信号
    for (const QString& tag : expiredTags) {
        emit dataExpired(tag);
    }
}

} // namespace ModbusPlexLink

