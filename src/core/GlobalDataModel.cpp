#include "GlobalDataModel.h"
#include <QDebug>

namespace ModbusPlexLink {

GlobalDataModel& GlobalDataModel::instance() {
    static GlobalDataModel instance;
    return instance;
}

GlobalDataModel::GlobalDataModel()
    : QObject(nullptr)
{
    qDebug() << "[GlobalDataModel] 全局数据模型已初始化";
}

void GlobalDataModel::updatePoint(const QString& channelName,
                                  const QString& collectorName,
                                  const QString& tagName,
                                  const DataPoint& point) {
    QString fullTagName = makeFullTagName(channelName, collectorName, tagName);
    
    bool isNew = false;
    {
        QWriteLocker locker(&m_lock);
        isNew = !m_dataCache.contains(fullTagName);
        m_dataCache[fullTagName] = point;
    }
    
    if (isNew) {
        emit tagAdded(fullTagName);
    }
    emit dataUpdated(fullTagName, point);
}

void GlobalDataModel::updatePoints(const QString& channelName,
                                   const QString& collectorName,
                                   const QHash<QString, DataPoint>& points) {
    QWriteLocker locker(&m_lock);
    
    for (auto it = points.constBegin(); it != points.constEnd(); ++it) {
        QString fullTagName = makeFullTagName(channelName, collectorName, it.key());
        bool isNew = !m_dataCache.contains(fullTagName);
        m_dataCache[fullTagName] = it.value();
        
        if (isNew) {
            // 解锁后发送信号以避免死锁
            locker.unlock();
            emit tagAdded(fullTagName);
            locker.relock();
        }
    }
    
    locker.unlock();
    
    // 发送数据更新信号
    for (auto it = points.constBegin(); it != points.constEnd(); ++it) {
        QString fullTagName = makeFullTagName(channelName, collectorName, it.key());
        emit dataUpdated(fullTagName, it.value());
    }
}

DataPoint GlobalDataModel::readPoint(const QString& fullTagName) const {
    QReadLocker locker(&m_lock);
    
    if (m_dataCache.contains(fullTagName)) {
        return m_dataCache[fullTagName];
    }
    
    // 返回无效数据点
    DataPoint invalidPoint;
    invalidPoint.quality = DataQuality::Bad;
    return invalidPoint;
}

DataPoint GlobalDataModel::readPoint(const QString& channelName,
                                     const QString& collectorName,
                                     const QString& tagName) const {
    QString fullTagName = makeFullTagName(channelName, collectorName, tagName);
    return readPoint(fullTagName);
}

QStringList GlobalDataModel::getAllTags() const {
    QReadLocker locker(&m_lock);
    return m_dataCache.keys();
}

QStringList GlobalDataModel::getAllChannels() const {
    QReadLocker locker(&m_lock);
    
    QSet<QString> channels;
    for (const QString& fullTag : m_dataCache.keys()) {
        QString channelName, collectorName, tagName;
        if (parseFullTagName(fullTag, channelName, collectorName, tagName)) {
            channels.insert(channelName);
        }
    }
    
    return channels.values();
}

QStringList GlobalDataModel::getTagsByChannel(const QString& channelName) const {
    QReadLocker locker(&m_lock);
    
    QStringList result;
    QString prefix = channelName + ":";
    
    for (const QString& fullTag : m_dataCache.keys()) {
        if (fullTag.startsWith(prefix)) {
            result.append(fullTag);
        }
    }
    
    return result;
}

QStringList GlobalDataModel::getTagsByCollector(const QString& channelName,
                                                 const QString& collectorName) const {
    QReadLocker locker(&m_lock);
    
    QStringList result;
    QString prefix = channelName + ":" + collectorName + ":";
    
    for (const QString& fullTag : m_dataCache.keys()) {
        if (fullTag.startsWith(prefix)) {
            result.append(fullTag);
        }
    }
    
    return result;
}

int GlobalDataModel::size() const {
    QReadLocker locker(&m_lock);
    return m_dataCache.size();
}

bool GlobalDataModel::contains(const QString& fullTagName) const {
    QReadLocker locker(&m_lock);
    return m_dataCache.contains(fullTagName);
}

void GlobalDataModel::clearChannelData(const QString& channelName) {
    QWriteLocker locker(&m_lock);
    
    QString prefix = channelName + ":";
    QStringList toRemove;
    
    for (const QString& fullTag : m_dataCache.keys()) {
        if (fullTag.startsWith(prefix)) {
            toRemove.append(fullTag);
        }
    }
    
    for (const QString& tag : toRemove) {
        m_dataCache.remove(tag);
    }
    
    locker.unlock();
    
    // 发送移除信号
    for (const QString& tag : toRemove) {
        emit tagRemoved(tag);
    }
    
    qDebug() << "[GlobalDataModel] 已清除通道数据:" << channelName << "移除" << toRemove.size() << "个标签";
}

void GlobalDataModel::clear() {
    QWriteLocker locker(&m_lock);
    
    QStringList allTags = m_dataCache.keys();
    m_dataCache.clear();
    
    locker.unlock();
    
    // 发送移除信号
    for (const QString& tag : allTags) {
        emit tagRemoved(tag);
    }
    
    qDebug() << "[GlobalDataModel] 已清除所有数据，移除" << allTags.size() << "个标签";
}

QString GlobalDataModel::makeFullTagName(const QString& channelName,
                                          const QString& collectorName,
                                          const QString& tagName) const {
    return QString("%1:%2:%3").arg(channelName, collectorName, tagName);
}

bool GlobalDataModel::parseFullTagName(const QString& fullTagName,
                                        QString& channelName,
                                        QString& collectorName,
                                        QString& tagName) const {
    QStringList parts = fullTagName.split(':');
    if (parts.size() >= 3) {
        channelName = parts[0];
        collectorName = parts[1];
        // 变量名可能包含冒号，所以取剩余部分
        tagName = parts.mid(2).join(':');
        return true;
    }
    return false;
}

} // namespace ModbusPlexLink
