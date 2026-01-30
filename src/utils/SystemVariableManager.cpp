#include "SystemVariableManager.h"
#include "core/ChannelManager.h"
#include "core/Channel.h"
#include <QDebug>
#include <QJsonArray>

namespace ModbusPlexLink {

SystemVariableManager& SystemVariableManager::instance() {
    static SystemVariableManager instance;
    return instance;
}

SystemVariableManager::SystemVariableManager()
    : QObject(nullptr)
{
    qDebug() << "[SystemVariableManager] 系统变量管理器已初始化";
}

void SystemVariableManager::syncFromChannels(ChannelManager* channelManager) {
    if (!channelManager) {
        qWarning() << "[SystemVariableManager] 通道管理器为空";
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // 记录旧变量用于比较
    QSet<QString> oldIds = QSet<QString>(m_variables.keys().begin(), m_variables.keys().end());
    QSet<QString> newIds;
    
    // 遍历所有通道
    for (const QString& channelName : channelManager->getChannelNames()) {
        Channel* channel = channelManager->getChannel(channelName);
        if (!channel) continue;
        
        // 只处理采集通道
        if (channel->getType() != ChannelType::Collector) continue;
        
        ChannelConfig config = channel->getConfig();
        
        // 遍历所有采集器
        for (const QJsonObject& collectorConfig : config.collectors) {
            QString collectorName = collectorConfig["name"].toString();
            if (collectorName.isEmpty()) continue;
            
            // 获取映射规则
            QJsonArray mappings = collectorConfig["mappings"].toArray();
            for (const QJsonValue& mappingVal : mappings) {
                QJsonObject mapping = mappingVal.toObject();
                QString tagName = mapping["tagName"].toString();
                if (tagName.isEmpty()) continue;
                
                QString fullId = makeFullId(channelName, collectorName, tagName);
                newIds.insert(fullId);
                
                // 创建或更新变量
                SystemVariable var;
                var.variableName = tagName;
                var.sourceChannel = channelName;
                var.sourceCollector = collectorName;
                var.fullId = fullId;
                var.dataType = static_cast<DataType>(mapping["dataType"].toInt(0));
                var.registerType = static_cast<RegisterType>(mapping["registerType"].toInt(3)); // 默认HoldingRegister
                var.comment = mapping["comment"].toString();
                var.unit = mapping["unit"].toString();
                var.updateTime = QDateTime::currentDateTime();
                
                m_variables[fullId] = var;
            }
        }
    }
    
    // 移除不再存在的变量
    QSet<QString> removedIds = oldIds - newIds;
    for (const QString& id : removedIds) {
        m_variables.remove(id);
    }
    
    locker.unlock();
    
    // 发送信号
    QSet<QString> addedIds = newIds - oldIds;
    for (const QString& id : addedIds) {
        emit variableAdded(id);
    }
    for (const QString& id : removedIds) {
        emit variableRemoved(id);
    }
    
    emit variablesUpdated();
    
    qDebug() << "[SystemVariableManager] 同步完成，变量数:" << m_variables.size()
             << "新增:" << addedIds.size() << "移除:" << removedIds.size();
}

QList<SystemVariable> SystemVariableManager::getAllVariables() const {
    QMutexLocker locker(&m_mutex);
    return m_variables.values();
}

int SystemVariableManager::getVariableCount() const {
    QMutexLocker locker(&m_mutex);
    return m_variables.size();
}

QList<SystemVariable> SystemVariableManager::getVariablesByChannel(const QString& channelName) const {
    QMutexLocker locker(&m_mutex);
    
    QList<SystemVariable> result;
    for (const SystemVariable& var : m_variables) {
        if (var.sourceChannel == channelName) {
            result.append(var);
        }
    }
    return result;
}

QList<SystemVariable> SystemVariableManager::getVariablesByCollector(const QString& channelName,
                                                                       const QString& collectorName) const {
    QMutexLocker locker(&m_mutex);
    
    QList<SystemVariable> result;
    for (const SystemVariable& var : m_variables) {
        if (var.sourceChannel == channelName && var.sourceCollector == collectorName) {
            result.append(var);
        }
    }
    return result;
}

QList<SystemVariable> SystemVariableManager::searchVariables(const QString& keyword) const {
    QMutexLocker locker(&m_mutex);
    
    QList<SystemVariable> result;
    QString lowerKeyword = keyword.toLower();
    
    for (const SystemVariable& var : m_variables) {
        // 在变量名、通道名、采集器名、注释中搜索
        if (var.variableName.toLower().contains(lowerKeyword) ||
            var.sourceChannel.toLower().contains(lowerKeyword) ||
            var.sourceCollector.toLower().contains(lowerKeyword) ||
            var.comment.toLower().contains(lowerKeyword) ||
            var.fullId.toLower().contains(lowerKeyword)) {
            result.append(var);
        }
    }
    return result;
}

SystemVariable SystemVariableManager::getVariable(const QString& fullId) const {
    QMutexLocker locker(&m_mutex);
    return m_variables.value(fullId, SystemVariable());
}

bool SystemVariableManager::hasVariable(const QString& fullId) const {
    QMutexLocker locker(&m_mutex);
    return m_variables.contains(fullId);
}

QStringList SystemVariableManager::getAllChannelNames() const {
    QMutexLocker locker(&m_mutex);
    
    QSet<QString> channels;
    for (const SystemVariable& var : m_variables) {
        channels.insert(var.sourceChannel);
    }
    return channels.values();
}

QStringList SystemVariableManager::getCollectorNames(const QString& channelName) const {
    QMutexLocker locker(&m_mutex);
    
    QSet<QString> collectors;
    for (const SystemVariable& var : m_variables) {
        if (var.sourceChannel == channelName) {
            collectors.insert(var.sourceCollector);
        }
    }
    return collectors.values();
}

void SystemVariableManager::clear() {
    QMutexLocker locker(&m_mutex);
    
    QStringList allIds = m_variables.keys();
    m_variables.clear();
    
    locker.unlock();
    
    for (const QString& id : allIds) {
        emit variableRemoved(id);
    }
    emit variablesUpdated();
    
    qDebug() << "[SystemVariableManager] 已清除所有变量";
}

QString SystemVariableManager::makeFullId(const QString& channelName,
                                           const QString& collectorName,
                                           const QString& variableName) const {
    return QString("%1:%2:%3").arg(channelName, collectorName, variableName);
}

} // namespace ModbusPlexLink
