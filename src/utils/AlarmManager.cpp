#include "AlarmManager.h"
#include "AlarmDatabase.h"
#include "core/ChannelManager.h"
#include "core/Channel.h"
#include "core/UniversalDataModel.h"
#include <QFile>
#include <QJsonDocument>
#include <QUuid>
#include <QDebug>
#include <QTextStream>
#include <QStringConverter>
#include <QStandardPaths>

namespace ModbusPlexLink {

// AlarmRule 序列化
QJsonObject AlarmRule::toJson() const {
    QJsonObject json;
    json["id"] = id;
    json["name"] = name;
    json["enabled"] = enabled;
    json["channelName"] = channelName;
    json["tagName"] = tagName;
    json["type"] = static_cast<int>(type);
    json["priority"] = static_cast<int>(priority);
    json["highLimit"] = highLimit;
    json["lowLimit"] = lowLimit;
    json["highHighLimit"] = highHighLimit;
    json["lowLowLimit"] = lowLowLimit;
    json["deadband"] = deadband;
    json["delaySeconds"] = delaySeconds;
    json["message"] = message;
    json["recordingConfig"] = recordingConfig.toJson();
    return json;
}

AlarmRule AlarmRule::fromJson(const QJsonObject& json) {
    AlarmRule rule;
    rule.id = json["id"].toString();
    rule.name = json["name"].toString();
    rule.enabled = json["enabled"].toBool(true);
    rule.channelName = json["channelName"].toString();
    rule.tagName = json["tagName"].toString();
    rule.type = static_cast<AlarmType>(json["type"].toInt());
    rule.priority = static_cast<AlarmPriority>(json["priority"].toInt());
    rule.highLimit = json["highLimit"].toDouble(100.0);
    rule.lowLimit = json["lowLimit"].toDouble(0.0);
    rule.highHighLimit = json["highHighLimit"].toDouble(120.0);
    rule.lowLowLimit = json["lowLowLimit"].toDouble(-10.0);
    rule.deadband = json["deadband"].toDouble(1.0);
    rule.delaySeconds = json["delaySeconds"].toInt(0);
    rule.message = json["message"].toString();
    if (json.contains("recordingConfig")) {
        rule.recordingConfig = AlarmRecordingConfig::fromJson(json["recordingConfig"].toObject());
    }
    return rule;
}

// AlarmEvent 序列化
QJsonObject AlarmEvent::toJson() const {
    QJsonObject json;
    json["id"] = id;
    json["ruleId"] = ruleId;
    json["ruleName"] = ruleName;
    json["type"] = static_cast<int>(type);
    json["priority"] = static_cast<int>(priority);
    json["state"] = static_cast<int>(state);
    json["channelName"] = channelName;
    json["tagName"] = tagName;
    json["value"] = value.toString();
    json["message"] = message;
    json["activeTime"] = activeTime.toString(Qt::ISODate);
    json["acknowledgedTime"] = acknowledgedTime.toString(Qt::ISODate);
    json["clearedTime"] = clearedTime.toString(Qt::ISODate);
    json["acknowledgedBy"] = acknowledgedBy;
    json["hasRecording"] = hasRecording;
    json["recordingId"] = recordingId;
    json["recordingFilePath"] = recordingFilePath;
    return json;
}

AlarmEvent AlarmEvent::fromJson(const QJsonObject& json) {
    AlarmEvent event;
    event.id = json["id"].toString();
    event.ruleId = json["ruleId"].toString();
    event.ruleName = json["ruleName"].toString();
    event.type = static_cast<AlarmType>(json["type"].toInt());
    event.priority = static_cast<AlarmPriority>(json["priority"].toInt());
    event.state = static_cast<AlarmState>(json["state"].toInt());
    event.channelName = json["channelName"].toString();
    event.tagName = json["tagName"].toString();
    event.value = json["value"];
    event.message = json["message"].toString();
    event.activeTime = QDateTime::fromString(json["activeTime"].toString(), Qt::ISODate);
    event.acknowledgedTime = QDateTime::fromString(json["acknowledgedTime"].toString(), Qt::ISODate);
    event.clearedTime = QDateTime::fromString(json["clearedTime"].toString(), Qt::ISODate);
    event.acknowledgedBy = json["acknowledgedBy"].toString();
    event.hasRecording = json["hasRecording"].toBool(false);
    event.recordingId = json["recordingId"].toString();
    event.recordingFilePath = json["recordingFilePath"].toString();
    return event;
}

// AlarmManager 实现
AlarmManager::AlarmManager(ChannelManager* channelManager, QObject *parent)
    : QObject(parent)
    , m_channelManager(channelManager)
    , m_maxHistorySize(1000)
    , m_recordingSampleTimer(nullptr)
    , m_preTriggerSampleTimer(nullptr)
    , m_recordingSaveDir(QString())
{
    if (!m_channelManager) {
        qWarning() << "AlarmManager: ChannelManager is null!";
        return;
    }
    
    // 初始化告警数据库
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dbPath.isEmpty()) {
        dbPath = ".";
    }
    QString alarmDbFile = dbPath + "/alarms.db";
    if (!AlarmDatabase::instance().initialize(alarmDbFile)) {
        qWarning() << "[AlarmManager] 告警数据库初始化失败，历史告警将不会持久化";
    } else {
        qInfo() << "[AlarmManager] 告警数据库已初始化:" << alarmDbFile;
    }
    
    // 初始化录波采样定时器（用于告警触发后的录波）
    m_recordingSampleTimer = new QTimer(this);
    connect(m_recordingSampleTimer, &QTimer::timeout, this, &AlarmManager::onRecordingTimer);
    
    // 初始化预触发采样定时器（持续采样，用于预触发缓冲区）
    m_preTriggerSampleTimer = new QTimer(this);
    connect(m_preTriggerSampleTimer, &QTimer::timeout, this, &AlarmManager::onPreTriggerSampleTimer);

    // 连接现有通道的信号
    for (const QString& channelName : m_channelManager->getChannelNames()) {
        Channel* channel = m_channelManager->getChannel(channelName);
        if (channel) {
            connectChannelSignals(channel);
        }
    }

    // 监听新通道创建
    connect(m_channelManager, &ChannelManager::channelCreated, this, [this](const QString& name) {
        Channel* channel = m_channelManager->getChannel(name);
        if (channel) {
            connectChannelSignals(channel);
        }
    });
}

AlarmManager::~AlarmManager() {
}

void AlarmManager::connectChannelSignals(Channel* channel) {
    if (!channel) return;

    // 监听数据更新
    if (channel->getDataModel()) {
        connect(channel->getDataModel(), &UniversalDataModel::dataUpdated,
                this, &AlarmManager::onDataUpdated, Qt::UniqueConnection);
    }

    // 监听采集器连接状态
    connect(channel, &Channel::collectorStateChanged,
            this, &AlarmManager::onCollectorConnectionChanged, Qt::UniqueConnection);
}

QString AlarmManager::addRule(const AlarmRule& rule) {
    AlarmRule newRule = rule;
    if (newRule.id.isEmpty()) {
        newRule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    {
        QMutexLocker locker(&m_rulesMutex);
        m_rules.append(newRule);
    }
    
    // 如果规则启用了录波，初始化预触发缓冲区并启动采样
    if (newRule.recordingConfig.enabled) {
        initPreTriggerBuffer(newRule);
        startPreTriggerSampling();
    }

    emit ruleAdded(newRule.id);
    qInfo() << "[AlarmManager] 添加报警规则:" << newRule.name << "(" << newRule.id << ")";

    return newRule.id;
}

bool AlarmManager::updateRule(const QString& ruleId, const AlarmRule& rule) {
    bool wasRecordingEnabled = false;
    
    {
        QMutexLocker locker(&m_rulesMutex);

        for (int i = 0; i < m_rules.size(); ++i) {
            if (m_rules[i].id == ruleId) {
                wasRecordingEnabled = m_rules[i].recordingConfig.enabled;
                m_rules[i] = rule;
                m_rules[i].id = ruleId;  // 保持ID不变
                break;
            }
        }
    }
    
    // 处理录波配置变化
    if (rule.recordingConfig.enabled && !wasRecordingEnabled) {
        // 新启用录波
        initPreTriggerBuffer(rule);
        startPreTriggerSampling();
    } else if (!rule.recordingConfig.enabled && wasRecordingEnabled) {
        // 禁用录波
        m_preTriggerBuffers.remove(ruleId);
        stopPreTriggerSampling();
    } else if (rule.recordingConfig.enabled) {
        // 更新录波配置
        m_preTriggerBuffers.remove(ruleId);
        initPreTriggerBuffer(rule);
    }
    
    emit ruleUpdated(ruleId);
    qInfo() << "[AlarmManager] 更新报警规则:" << rule.name << "(" << ruleId << ")";
    return true;
}

bool AlarmManager::removeRule(const QString& ruleId) {
    QString name;
    
    {
        QMutexLocker locker(&m_rulesMutex);

        for (int i = 0; i < m_rules.size(); ++i) {
            if (m_rules[i].id == ruleId) {
                name = m_rules[i].name;
                m_rules.removeAt(i);
                break;
            }
        }
    }
    
    if (name.isEmpty()) {
        qWarning() << "[AlarmManager] 未找到规则:" << ruleId;
        return false;
    }

    // 清除该规则的活动报警和缓存
    m_activeAlarmsMap.remove(ruleId);
    m_delayCache.remove(ruleId);
    
    // 清除预触发缓冲区
    m_preTriggerBuffers.remove(ruleId);
    stopPreTriggerSampling();

    emit ruleRemoved(ruleId);
    qInfo() << "[AlarmManager] 删除报警规则:" << name << "(" << ruleId << ")";
    return true;
}

AlarmRule AlarmManager::getRule(const QString& ruleId) const {
    QMutexLocker locker(&m_rulesMutex);

    for (const AlarmRule& rule : m_rules) {
        if (rule.id == ruleId) {
            return rule;
        }
    }

    return AlarmRule();  // 返回空规则
}

QList<AlarmRule> AlarmManager::getAllRules() const {
    QMutexLocker locker(&m_rulesMutex);
    return m_rules;
}

void AlarmManager::clearAllRules() {
    QMutexLocker locker(&m_rulesMutex);
    m_rules.clear();
    m_activeAlarmsMap.clear();
    m_delayCache.clear();
    qInfo() << "[AlarmManager] 清除所有报警规则";
}

bool AlarmManager::enableRule(const QString& ruleId, bool enable) {
    QMutexLocker locker(&m_rulesMutex);

    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].id == ruleId) {
            m_rules[i].enabled = enable;
            qInfo() << "[AlarmManager] 规则" << m_rules[i].name
                    << (enable ? "已启用" : "已禁用");
            return true;
        }
    }

    return false;
}

QList<AlarmEvent> AlarmManager::getActiveAlarms() const {
    QMutexLocker locker(&m_alarmsMutex);
    return m_activeAlarms;
}

QList<AlarmEvent> AlarmManager::getAlarmHistory(const QDateTime& start, const QDateTime& end) const {
    // 优先从数据库查询
    if (AlarmDatabase::instance().isInitialized()) {
        return AlarmDatabase::instance().getAlarmHistory(start, end);
    }
    
    // 数据库不可用时，从内存查询（兼容旧逻辑）
    QMutexLocker locker(&m_alarmsMutex);

    QList<AlarmEvent> filtered;
    for (const AlarmEvent& event : m_alarmHistory) {
        if (event.activeTime >= start && event.activeTime <= end) {
            filtered.append(event);
        }
    }

    return filtered;
}

AlarmEvent AlarmManager::getAlarmEvent(const QString& eventId) const {
    QMutexLocker locker(&m_alarmsMutex);

    // 先在活动报警中查找
    for (const AlarmEvent& event : m_activeAlarms) {
        if (event.id == eventId) {
            return event;
        }
    }

    // 再在历史中查找
    for (const AlarmEvent& event : m_alarmHistory) {
        if (event.id == eventId) {
            return event;
        }
    }

    return AlarmEvent();  // 返回空事件
}

bool AlarmManager::acknowledgeAlarm(const QString& eventId, const QString& acknowledgedBy) {
    bool found = false;
    QString ackBy;
    AlarmEvent updatedEvent;

    // 在锁内修改状态
    {
        QMutexLocker locker(&m_alarmsMutex);

        for (int i = 0; i < m_activeAlarms.size(); ++i) {
            if (m_activeAlarms[i].id == eventId) {
                m_activeAlarms[i].state = AlarmState::Acknowledged;
                m_activeAlarms[i].acknowledgedTime = QDateTime::currentDateTime();
                m_activeAlarms[i].acknowledgedBy = acknowledgedBy.isEmpty() ? tr("用户") : acknowledgedBy;
                ackBy = m_activeAlarms[i].acknowledgedBy;
                updatedEvent = m_activeAlarms[i];
                found = true;
                break;
            }
        }
    }

    // 在锁外发射信号和更新数据库
    if (found) {
        // 更新数据库
        if (AlarmDatabase::instance().isInitialized()) {
            AlarmDatabase::instance().updateAlarmEvent(updatedEvent);
        }
        
        emit alarmAcknowledged(eventId);
        qInfo() << "[AlarmManager] 报警已确认:" << eventId << "by" << ackBy;
    }

    return found;
}

bool AlarmManager::clearAlarm(const QString& eventId) {
    bool found = false;
    AlarmEvent clearedEvent;

    // 在锁内执行清除操作
    {
        QMutexLocker locker(&m_alarmsMutex);

        for (int i = 0; i < m_activeAlarms.size(); ++i) {
            if (m_activeAlarms[i].id == eventId) {
                m_activeAlarms[i].state = AlarmState::Cleared;
                m_activeAlarms[i].clearedTime = QDateTime::currentDateTime();
                clearedEvent = m_activeAlarms[i];

                // 移到内存历史（兼容）
                m_alarmHistory.prepend(m_activeAlarms[i]);
                if (m_alarmHistory.size() > m_maxHistorySize) {
                    m_alarmHistory.removeLast();
                }

                m_activeAlarms.removeAt(i);
                found = true;
                break;
            }
        }
    }

    // 在锁外发射信号和更新数据库
    if (found) {
        // 更新数据库
        if (AlarmDatabase::instance().isInitialized()) {
            AlarmDatabase::instance().updateAlarmEvent(clearedEvent);
        }
        
        emit alarmCleared(eventId);
        qInfo() << "[AlarmManager] 报警已清除:" << eventId;
    }

    return found;
}

int AlarmManager::getActiveAlarmCount() const {
    QMutexLocker locker(&m_alarmsMutex);
    return m_activeAlarms.size();
}

int AlarmManager::getAlarmCount(AlarmPriority priority) const {
    QMutexLocker locker(&m_alarmsMutex);

    int count = 0;
    for (const AlarmEvent& event : m_activeAlarms) {
        if (event.priority == priority) {
            count++;
        }
    }
    return count;
}

int AlarmManager::getTotalAlarmCount() const {
    QMutexLocker locker(&m_alarmsMutex);
    return m_activeAlarms.size() + m_alarmHistory.size();
}

void AlarmManager::onDataUpdated(const QString& tagName, const DataPoint& point) {
    // 获取通道名称
    UniversalDataModel* udm = qobject_cast<UniversalDataModel*>(sender());
    if (!udm || !m_channelManager) return;

    QString channelName;
    for (const QString& name : m_channelManager->getChannelNames()) {
        Channel* channel = m_channelManager->getChannel(name);
        if (channel && channel->getDataModel() == udm) {
            channelName = name;
            break;
        }
    }

    if (channelName.isEmpty()) return;

    // 检查报警条件
    checkAlarmConditions(channelName, tagName, point);
}

void AlarmManager::onCollectorConnectionChanged(const QString& collectorName, bool connected) {
    // 处理连接丢失报警
    Channel* channel = qobject_cast<Channel*>(sender());
    if (!channel) return;

    QString channelName = channel->getName();

    QMutexLocker locker(&m_rulesMutex);
    for (const AlarmRule& rule : m_rules) {
        if (!rule.enabled) continue;
        if (rule.channelName != channelName) continue;
        if (rule.type != AlarmType::ConnectionLost) continue;

        if (!connected) {
            // 触发连接丢失报警
            triggerAlarm(rule, channelName, "Connection", QVariant("Lost"));
        } else {
            // 自动清除连接恢复
            autoClearAlarm(rule.id);
        }
    }
}

void AlarmManager::checkAlarmConditions(const QString& channelName,
                                         const QString& tagName,
                                         const DataPoint& point) {
    QMutexLocker locker(&m_rulesMutex);

    for (const AlarmRule& rule : m_rules) {
        if (!rule.enabled) continue;
        if (rule.channelName != channelName && rule.channelName != "*") continue;
        if (rule.tagName != tagName) continue;

        bool shouldTrigger = false;
        double value = point.value.toDouble();

        switch (rule.type) {
            case AlarmType::HighHighLimit:
                if (value > rule.highHighLimit) shouldTrigger = true;
                break;

            case AlarmType::HighLimit:
                if (value > rule.highLimit) shouldTrigger = true;
                break;

            case AlarmType::LowLimit:
                if (value < rule.lowLimit) shouldTrigger = true;
                break;

            case AlarmType::LowLowLimit:
                if (value < rule.lowLowLimit) shouldTrigger = true;
                break;

            case AlarmType::DataQuality:
                if (point.quality != DataQuality::Good) shouldTrigger = true;
                break;

            case AlarmType::ValueChange:
                // 值变化检测（需要存储上次值）
                // TODO: 实现值变化检测
                break;

            default:
                break;
        }

        if (shouldTrigger) {
            // 处理延时
            if (rule.delaySeconds > 0) {
                if (!m_delayCache.contains(rule.id)) {
                    // 第一次触发，记录时间
                    DelayEntry entry;
                    entry.firstTriggerTime = QDateTime::currentDateTime();
                    entry.value = point.value;
                    m_delayCache[rule.id] = entry;
                } else {
                    // 检查延时是否满足
                    DelayEntry& entry = m_delayCache[rule.id];
                    qint64 elapsed = entry.firstTriggerTime.secsTo(QDateTime::currentDateTime());
                    if (elapsed >= rule.delaySeconds) {
                        // 延时满足，触发报警
                        triggerAlarm(rule, channelName, tagName, point.value);
                        m_delayCache.remove(rule.id);  // 清除延时缓存
                    }
                }
            } else {
                // 无延时，立即触发
                triggerAlarm(rule, channelName, tagName, point.value);
            }
        } else {
            // 条件不满足，清除延时缓存
            m_delayCache.remove(rule.id);

            // 如果有活动报警，考虑自动清除（带死区）
            if (m_activeAlarmsMap.contains(rule.id)) {
                bool shouldClear = false;

                switch (rule.type) {
                    case AlarmType::HighLimit:
                        if (value < (rule.highLimit - rule.deadband)) shouldClear = true;
                        break;
                    case AlarmType::LowLimit:
                        if (value > (rule.lowLimit + rule.deadband)) shouldClear = true;
                        break;
                    case AlarmType::HighHighLimit:
                        if (value < (rule.highHighLimit - rule.deadband)) shouldClear = true;
                        break;
                    case AlarmType::LowLowLimit:
                        if (value > (rule.lowLowLimit + rule.deadband)) shouldClear = true;
                        break;
                    default:
                        break;
                }

                if (shouldClear) {
                    autoClearAlarm(rule.id);
                }
            }
        }
    }
}

void AlarmManager::triggerAlarm(const AlarmRule& rule,
                                 const QString& channelName,
                                 const QString& tagName,
                                 const QVariant& value) {
    // 检查是否已存在活动报警（避免重复触发）
    if (m_activeAlarmsMap.contains(rule.id)) {
        return;  // 已有活动报警，不重复触发
    }

    AlarmEvent event;
    event.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event.ruleId = rule.id;
    event.ruleName = rule.name;
    event.type = rule.type;
    event.priority = rule.priority;
    event.state = AlarmState::Active;
    event.channelName = channelName;
    event.tagName = tagName;
    event.value = value;
    event.message = generateAlarmMessage(rule, value);
    event.activeTime = QDateTime::currentDateTime();
    
    // 如果规则启用了录波，启动录波
    if (rule.recordingConfig.enabled) {
        startRecordingForAlarm(event, rule);
    }

    // 在锁内添加到列表
    {
        QMutexLocker locker(&m_alarmsMutex);
        m_activeAlarms.append(event);
        m_activeAlarmsMap[rule.id] = event;
    }
    
    // 持久化到数据库
    if (AlarmDatabase::instance().isInitialized()) {
        AlarmDatabase::instance().insertAlarmEvent(event);
    }

    // 在锁外发射信号，避免死锁
    emit alarmTriggered(event);

    qWarning() << "[AlarmManager] *** 报警触发 ***"
               << "规则:" << rule.name
               << "| 标签:" << tagName
               << "| 值:" << value.toString()
               << "| 消息:" << event.message
               << (event.hasRecording ? "| 录波已启动" : "");
}

void AlarmManager::autoClearAlarm(const QString& ruleId) {
    if (!m_activeAlarmsMap.contains(ruleId)) return;

    QString eventId;
    bool hasRecording = false;
    AlarmEvent clearedEvent;

    // 在锁内执行清除操作
    {
        QMutexLocker locker(&m_alarmsMutex);

        AlarmEvent& event = m_activeAlarmsMap[ruleId];
        eventId = event.id;
        hasRecording = event.hasRecording;

        // 从活动报警中移除
        for (int i = 0; i < m_activeAlarms.size(); ++i) {
            if (m_activeAlarms[i].id == eventId) {
                m_activeAlarms[i].state = AlarmState::Cleared;
                m_activeAlarms[i].clearedTime = QDateTime::currentDateTime();
                clearedEvent = m_activeAlarms[i];

                // 移到内存历史（兼容）
                m_alarmHistory.prepend(m_activeAlarms[i]);
                if (m_alarmHistory.size() > m_maxHistorySize) {
                    m_alarmHistory.removeLast();
                }

                m_activeAlarms.removeAt(i);
                break;
            }
        }

        m_activeAlarmsMap.remove(ruleId);
    }
    
    // 如果有录波，停止录波
    if (hasRecording && m_activeSessions.contains(eventId)) {
        stopRecordingForAlarm(eventId);
    }
    
    // 更新数据库
    if (AlarmDatabase::instance().isInitialized() && !clearedEvent.id.isEmpty()) {
        AlarmDatabase::instance().updateAlarmEvent(clearedEvent);
    }

    // 在锁外发射信号
    emit alarmCleared(eventId);
    qInfo() << "[AlarmManager] 报警自动清除:" << eventId;
}

QString AlarmManager::generateAlarmMessage(const AlarmRule& rule, const QVariant& value) const {
    QString msg = rule.message;

    if (msg.isEmpty()) {
        // 默认消息
        switch (rule.type) {
            case AlarmType::HighHighLimit:
                msg = tr("%1 超过高高限 (%2 > %3)").arg(rule.tagName).arg(value.toString()).arg(rule.highHighLimit);
                break;
            case AlarmType::HighLimit:
                msg = tr("%1 超过高限 (%2 > %3)").arg(rule.tagName).arg(value.toString()).arg(rule.highLimit);
                break;
            case AlarmType::LowLimit:
                msg = tr("%1 低于低限 (%2 < %3)").arg(rule.tagName).arg(value.toString()).arg(rule.lowLimit);
                break;
            case AlarmType::LowLowLimit:
                msg = tr("%1 低于低低限 (%2 < %3)").arg(rule.tagName).arg(value.toString()).arg(rule.lowLowLimit);
                break;
            case AlarmType::DataQuality:
                msg = tr("%1 数据质量异常").arg(rule.tagName);
                break;
            case AlarmType::ConnectionLost:
                msg = tr("%1 连接丢失").arg(rule.channelName);
                break;
            default:
                msg = tr("%1 触发报警").arg(rule.tagName);
                break;
        }
    } else {
        // 替换占位符
        msg.replace("{tagName}", rule.tagName);
        msg.replace("{value}", value.toString());
        msg.replace("{channelName}", rule.channelName);
    }

    return msg;
}

bool AlarmManager::loadConfig(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[AlarmManager] 无法打开配置文件:" << filename;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "[AlarmManager] 配置文件格式错误";
        return false;
    }

    fromJson(doc.object());
    qInfo() << "[AlarmManager] 配置加载成功:" << filename;
    return true;
}

bool AlarmManager::saveConfig(const QString& filename) const {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[AlarmManager] 无法创建配置文件:" << filename;
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qInfo() << "[AlarmManager] 配置保存成功:" << filename;
    return true;
}

QJsonObject AlarmManager::toJson() const {
    QMutexLocker locker(&m_rulesMutex);

    QJsonObject json;
    json["version"] = "1.0";
    json["maxHistorySize"] = m_maxHistorySize;

    QJsonArray rulesArray;
    for (const AlarmRule& rule : m_rules) {
        rulesArray.append(rule.toJson());
    }
    json["rules"] = rulesArray;

    return json;
}

void AlarmManager::fromJson(const QJsonObject& json) {
    bool hasRecordingRules = false;
    
    {
        QMutexLocker locker(&m_rulesMutex);

        m_rules.clear();
        m_preTriggerBuffers.clear();  // 清除旧的缓冲区
        m_maxHistorySize = json["maxHistorySize"].toInt(1000);

        QJsonArray rulesArray = json["rules"].toArray();
        for (const QJsonValue& val : rulesArray) {
            if (val.isObject()) {
                AlarmRule rule = AlarmRule::fromJson(val.toObject());
                m_rules.append(rule);
                
                // 如果规则启用了录波，初始化预触发缓冲
                if (rule.recordingConfig.enabled) {
                    initPreTriggerBuffer(rule);
                    hasRecordingRules = true;
                }
            }
        }
    }
    
    // 启动预触发采样
    if (hasRecordingRules) {
        startPreTriggerSampling();
    }

    qInfo() << "[AlarmManager] 加载" << m_rules.size() << "条报警规则";
}

// ==================== 录波相关方法实现 ====================

void AlarmManager::setRecordingSaveDirectory(const QString& directory) {
    m_recordingSaveDir = directory;
}

AlarmRecordingData AlarmManager::getRecordingData(const QString& alarmEventId) const {
    if (m_completedRecordings.contains(alarmEventId)) {
        return m_completedRecordings[alarmEventId];
    }
    return AlarmRecordingData();
}

QList<AlarmEvent> AlarmManager::getAlarmsWithRecording() const {
    QMutexLocker locker(&m_alarmsMutex);
    QList<AlarmEvent> result;
    
    // 从活动报警中查找
    for (const AlarmEvent& event : m_activeAlarms) {
        if (event.hasRecording) {
            result.append(event);
        }
    }
    
    // 从历史报警中查找
    for (const AlarmEvent& event : m_alarmHistory) {
        if (event.hasRecording) {
            result.append(event);
        }
    }
    
    return result;
}

bool AlarmManager::exportRecordingToCsv(const QString& alarmEventId, const QString& filename) {
    if (!m_completedRecordings.contains(alarmEventId)) {
        qWarning() << "[AlarmManager] 录波数据不存在:" << alarmEventId;
        return false;
    }
    
    const AlarmRecordingData& data = m_completedRecordings[alarmEventId];
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[AlarmManager] 无法创建文件:" << filename;
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    // 写入表头 - 使用可读的日期时间格式
    stream << "DateTime,Timestamp(ms)";
    for (const QString& tag : data.recordedTags) {
        stream << "," << tag;
    }
    stream << "\n";
    
    // 写入数据
    for (const auto& point : data.data) {
        // 转换时间戳为可读的日期时间格式
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(point.timestamp);
        stream << dt.toString("yyyy-MM-dd HH:mm:ss.zzz");
        stream << "," << point.timestamp;
        for (const QString& tag : data.recordedTags) {
            stream << ",";
            if (point.values.contains(tag)) {
                stream << QString::number(point.values[tag], 'f', 6);
            }
        }
        stream << "\n";
    }
    
    file.close();
    qInfo() << "[AlarmManager] 录波数据导出成功:" << filename << "数据点:" << data.data.size();
    return true;
}

void AlarmManager::cleanupOldRecordings(int keepDays) {
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-keepDays);
    
    QList<QString> toRemove;
    for (auto it = m_completedRecordings.begin(); it != m_completedRecordings.end(); ++it) {
        if (it.value().endTime < cutoff) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& id : toRemove) {
        m_completedRecordings.remove(id);
    }
    
    if (!toRemove.isEmpty()) {
        qInfo() << "[AlarmManager] 清理了" << toRemove.size() << "条过期录波数据";
    }
}

void AlarmManager::saveRecordingToCsv(const QString& alarmEventId) {
    // 内部方法：自动保存录波数据到默认位置
    if (!m_completedRecordings.contains(alarmEventId)) return;
    
    QString saveDir = m_recordingSaveDir.isEmpty() ? "." : m_recordingSaveDir;
    QString filename = QString("%1/alarm_recording_%2_%3.csv")
        .arg(saveDir)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"))
        .arg(alarmEventId.left(8));
    
    if (exportRecordingToCsv(alarmEventId, filename)) {
        m_completedRecordings[alarmEventId].csvFilePath = filename;
    }
}

void AlarmManager::sampleRecordingData() {
    // 为所有活动录波会话采样数据
    onRecordingTimer();
}

void AlarmManager::initPreTriggerBuffer(const AlarmRule& rule) {
    if (!rule.recordingConfig.enabled) return;
    
    PreTriggerBuffer buffer;
    buffer.config = rule.recordingConfig;
    buffer.maxSize = static_cast<int>(
        rule.recordingConfig.preTriggerSeconds * 1000.0 / 
        rule.recordingConfig.sampleIntervalMs);
    buffer.sampleTimer = nullptr;
    
    // 确定要录波的标签
    if (rule.recordingConfig.recordTags.isEmpty()) {
        buffer.tags.append(rule.tagName);
    } else {
        buffer.tags = rule.recordingConfig.recordTags;
    }
    
    m_preTriggerBuffers[rule.id] = buffer;
    
    qDebug() << "[AlarmManager] 初始化预触发缓冲:" << rule.name 
             << "缓冲区大小:" << buffer.maxSize;
}

void AlarmManager::updatePreTriggerBuffer(const QString& ruleId) {
    if (!m_preTriggerBuffers.contains(ruleId)) return;
    if (!m_channelManager) return;
    
    PreTriggerBuffer& buffer = m_preTriggerBuffers[ruleId];
    
    // 查找对应的规则获取通道名
    QString channelName;
    {
        QMutexLocker locker(&m_rulesMutex);
        for (const AlarmRule& rule : m_rules) {
            if (rule.id == ruleId) {
                channelName = rule.channelName;
                break;
            }
        }
    }
    
    if (channelName.isEmpty()) return;
    
    Channel* channel = m_channelManager->getChannel(channelName);
    if (!channel) return;
    
    UniversalDataModel* udm = channel->getDataModel();
    if (!udm) return;
    
    // 采样数据
    AlarmRecordingData::DataPoint point;
    point.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    bool hasData = false;
    for (const QString& tag : buffer.tags) {
        DataPoint dp = udm->readPoint(tag);
        if (dp.quality == DataQuality::Good || dp.quality == DataQuality::Uncertain) {
            point.values[tag] = dp.value.toDouble();
            hasData = true;
        }
    }
    
    // 只有有数据时才添加到缓冲区
    if (hasData) {
        buffer.buffer.append(point);
        
        // 限制缓冲区大小
        while (buffer.buffer.size() > buffer.maxSize) {
            buffer.buffer.removeFirst();
        }
    }
}

void AlarmManager::startRecordingForAlarm(AlarmEvent& event, const AlarmRule& rule) {
    if (!rule.recordingConfig.enabled) return;
    
    RecordingSession session;
    session.alarmEventId = event.id;
    session.ruleId = rule.id;
    session.config = rule.recordingConfig;
    session.startTime = QDateTime::currentDateTime();
    session.isActive = true;
    session.postTriggerTimer = nullptr;
    
    // 将预触发缓冲区数据刷入会话
    flushPreTriggerBuffer(rule.id, session);
    
    m_activeSessions[event.id] = session;
    
    // 启动录波采样定时器（如果尚未启动）
    if (!m_recordingSampleTimer->isActive()) {
        m_recordingSampleTimer->start(rule.recordingConfig.sampleIntervalMs);
    }
    
    // 更新事件的录波标志
    event.hasRecording = true;
    event.recordingId = event.id;
    
    emit recordingStarted(event.id);
    
    qInfo() << "[AlarmManager] 开始告警录波:" << rule.name 
            << "事件ID:" << event.id;
}

void AlarmManager::stopRecordingForAlarm(const QString& alarmEventId) {
    if (!m_activeSessions.contains(alarmEventId)) return;
    
    RecordingSession& session = m_activeSessions[alarmEventId];
    session.isActive = false;
    
    // 创建完成的录波数据
    AlarmRecordingData recording;
    recording.alarmEventId = alarmEventId;
    recording.startTime = session.startTime;
    recording.endTime = QDateTime::currentDateTime();
    recording.data = session.data;
    recording.isComplete = true;
    
    // 确定录波的标签
    if (m_preTriggerBuffers.contains(session.ruleId)) {
        recording.recordedTags = m_preTriggerBuffers[session.ruleId].tags;
    }
    recording.totalDataPoints = recording.data.size();
    
    // 先保存到内存，这样后续导出可以正常工作
    m_completedRecordings[alarmEventId] = recording;
    
    AlarmEvent eventToUpdate;
    bool needUpdateDb = false;
    
    // 自动导出CSV
    if (session.config.autoExportCsv) {
        QString saveDir = session.config.saveDirectory.isEmpty() 
                            ? m_recordingSaveDir : session.config.saveDirectory;
        if (saveDir.isEmpty()) {
            saveDir = ".";
        }
        
        QString filename = QString("%1/alarm_recording_%2_%3.csv")
            .arg(saveDir)
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"))
            .arg(alarmEventId.left(8));
        
        if (exportRecordingToCsv(alarmEventId, filename)) {
            m_completedRecordings[alarmEventId].csvFilePath = filename;
            recording.csvFilePath = filename;
            
            // 更新事件的录波文件路径
            QMutexLocker locker(&m_alarmsMutex);
            for (AlarmEvent& event : m_activeAlarms) {
                if (event.id == alarmEventId) {
                    event.recordingFilePath = filename;
                    eventToUpdate = event;
                    needUpdateDb = true;
                    break;
                }
            }
            for (AlarmEvent& event : m_alarmHistory) {
                if (event.id == alarmEventId) {
                    event.recordingFilePath = filename;
                    eventToUpdate = event;
                    needUpdateDb = true;
                    break;
                }
            }
        }
    }
    
    // 更新数据库中的录波文件路径
    if (needUpdateDb && AlarmDatabase::instance().isInitialized()) {
        AlarmDatabase::instance().updateAlarmEvent(eventToUpdate);
    }
    
    // 如果不保留在内存中，导出后删除
    if (!session.config.keepInMemory) {
        m_completedRecordings.remove(alarmEventId);
    }
    
    // 移除活动会话
    m_activeSessions.remove(alarmEventId);
    
    // 如果没有其他活动会话，停止定时器
    if (m_activeSessions.isEmpty()) {
        m_recordingSampleTimer->stop();
    }
    
    emit recordingCompleted(alarmEventId, recording.csvFilePath);
    emit recordingDataAvailable(alarmEventId);
    
    qInfo() << "[AlarmManager] 告警录波完成:" << alarmEventId 
            << "数据点:" << recording.totalDataPoints;
}

void AlarmManager::flushPreTriggerBuffer(const QString& ruleId, RecordingSession& session) {
    if (!m_preTriggerBuffers.contains(ruleId)) return;
    
    PreTriggerBuffer& buffer = m_preTriggerBuffers[ruleId];
    
    // 将缓冲区数据复制到会话
    session.data = buffer.buffer;
    
    qDebug() << "[AlarmManager] 刷新预触发缓冲区:" << buffer.buffer.size() << "个数据点";
}

void AlarmManager::onRecordingTimer() {
    if (!m_channelManager) return;
    
    // 只为活动录波会话采样数据（预触发缓冲区由onPreTriggerSampleTimer处理）
    for (auto it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it) {
        RecordingSession& session = it.value();
        if (!session.isActive) continue;
        
        // 查找规则和通道
        QString channelName;
        QStringList tags;
        {
            QMutexLocker locker(&m_rulesMutex);
            for (const AlarmRule& rule : m_rules) {
                if (rule.id == session.ruleId) {
                    channelName = rule.channelName;
                    // 获取要录波的标签
                    if (rule.recordingConfig.recordTags.isEmpty()) {
                        tags.append(rule.tagName);
                    } else {
                        tags = rule.recordingConfig.recordTags;
                    }
                    break;
                }
            }
        }
        
        if (channelName.isEmpty()) continue;
        
        Channel* channel = m_channelManager->getChannel(channelName);
        if (!channel) continue;
        
        UniversalDataModel* udm = channel->getDataModel();
        if (!udm) continue;
        
        // 采样数据
        AlarmRecordingData::DataPoint point;
        point.timestamp = QDateTime::currentMSecsSinceEpoch();
        
        bool hasData = false;
        for (const QString& tag : tags) {
            DataPoint dp = udm->readPoint(tag);
            if (dp.quality == DataQuality::Good || dp.quality == DataQuality::Uncertain) {
                point.values[tag] = dp.value.toDouble();
                hasData = true;
            }
        }
        
        // 只有有数据时才添加
        if (hasData) {
            session.data.append(point);
        }
        
        // 发送进度信号
        double elapsed = session.startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
        emit recordingProgress(it.key(), session.data.size(), elapsed);
    }
}

void AlarmManager::onPostTriggerTimeout() {
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    
    // 查找对应的会话
    for (auto it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it) {
        if (it.value().postTriggerTimer == timer) {
            stopRecordingForAlarm(it.key());
            break;
        }
    }
}

void AlarmManager::onPreTriggerSampleTimer() {
    // 持续为所有启用录波的规则采样预触发数据
    for (auto it = m_preTriggerBuffers.begin(); it != m_preTriggerBuffers.end(); ++it) {
        updatePreTriggerBuffer(it.key());
    }
}

void AlarmManager::startPreTriggerSampling() {
    if (m_preTriggerSampleTimer && !m_preTriggerSampleTimer->isActive()) {
        // 使用最小的采样间隔（100ms）来确保所有缓冲区都能及时更新
        int minInterval = 100;
        for (auto it = m_preTriggerBuffers.begin(); it != m_preTriggerBuffers.end(); ++it) {
            if (it.value().config.sampleIntervalMs < minInterval) {
                minInterval = it.value().config.sampleIntervalMs;
            }
        }
        m_preTriggerSampleTimer->start(minInterval);
        qInfo() << "[AlarmManager] 预触发采样已启动, 间隔:" << minInterval << "ms";
    }
}

void AlarmManager::stopPreTriggerSampling() {
    if (m_preTriggerSampleTimer && m_preTriggerSampleTimer->isActive()) {
        // 只有在没有任何需要预触发采样的规则时才停止
        if (m_preTriggerBuffers.isEmpty()) {
            m_preTriggerSampleTimer->stop();
            qInfo() << "[AlarmManager] 预触发采样已停止";
        }
    }
}

} // namespace ModbusPlexLink
