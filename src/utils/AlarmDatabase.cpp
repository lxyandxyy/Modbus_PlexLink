#include "AlarmDatabase.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace ModbusPlexLink {

AlarmDatabase& AlarmDatabase::instance() {
    static AlarmDatabase instance;
    return instance;
}

AlarmDatabase::AlarmDatabase()
    : QObject(nullptr)
    , m_initialized(false)
    , m_connectionName("AlarmDatabaseConnection")
{
}

AlarmDatabase::~AlarmDatabase() {
    close();
}

bool AlarmDatabase::initialize(const QString& dbPath) {
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        qWarning() << "[AlarmDatabase] 数据库已经初始化";
        return true;
    }
    
    // 确保目录存在
    QFileInfo fileInfo(dbPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "[AlarmDatabase] 无法创建目录:" << dir.absolutePath();
            emit databaseError(tr("无法创建数据库目录"));
            return false;
        }
    }
    
    // 创建数据库连接
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);
    
    if (!m_db.open()) {
        QString error = m_db.lastError().text();
        qWarning() << "[AlarmDatabase] 无法打开数据库:" << error;
        emit databaseError(tr("无法打开数据库: %1").arg(error));
        return false;
    }
    
    // 创建表
    if (!createTables()) {
        qWarning() << "[AlarmDatabase] 创建表失败";
        m_db.close();
        return false;
    }
    
    m_initialized = true;
    qInfo() << "[AlarmDatabase] 数据库初始化成功:" << dbPath;
    return true;
}

void AlarmDatabase::close() {
    QMutexLocker locker(&m_mutex);
    
    if (m_db.isOpen()) {
        m_db.close();
    }
    
    // 移除连接
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    
    m_initialized = false;
    qInfo() << "[AlarmDatabase] 数据库已关闭";
}

bool AlarmDatabase::createTables() {
    QSqlQuery query(m_db);
    
    // 创建告警事件表
    QString createEventsTable = R"(
        CREATE TABLE IF NOT EXISTS alarm_events (
            id TEXT PRIMARY KEY,
            rule_id TEXT NOT NULL,
            rule_name TEXT,
            alarm_type INTEGER,
            priority INTEGER,
            state INTEGER,
            channel_name TEXT,
            tag_name TEXT,
            trigger_value TEXT,
            message TEXT,
            active_time DATETIME,
            acknowledged_time DATETIME,
            cleared_time DATETIME,
            acknowledged_by TEXT,
            has_recording INTEGER DEFAULT 0,
            recording_id TEXT,
            recording_file_path TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createEventsTable)) {
        qWarning() << "[AlarmDatabase] 创建告警事件表失败:" << query.lastError().text();
        emit databaseError(tr("创建告警事件表失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    // 创建索引
    QStringList indexQueries = {
        "CREATE INDEX IF NOT EXISTS idx_alarm_events_active_time ON alarm_events(active_time)",
        "CREATE INDEX IF NOT EXISTS idx_alarm_events_channel ON alarm_events(channel_name)",
        "CREATE INDEX IF NOT EXISTS idx_alarm_events_state ON alarm_events(state)",
        "CREATE INDEX IF NOT EXISTS idx_alarm_events_has_recording ON alarm_events(has_recording)"
    };
    
    for (const QString& indexQuery : indexQueries) {
        if (!query.exec(indexQuery)) {
            qWarning() << "[AlarmDatabase] 创建索引失败:" << query.lastError().text();
            // 索引创建失败不是致命错误，继续执行
        }
    }
    
    // 创建告警规则表（可选）
    QString createRulesTable = R"(
        CREATE TABLE IF NOT EXISTS alarm_rules (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            enabled INTEGER DEFAULT 1,
            channel_name TEXT,
            tag_name TEXT,
            alarm_type INTEGER,
            priority INTEGER,
            high_limit REAL,
            low_limit REAL,
            high_high_limit REAL,
            low_low_limit REAL,
            deadband REAL,
            delay_seconds INTEGER,
            message TEXT,
            recording_config TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createRulesTable)) {
        qWarning() << "[AlarmDatabase] 创建告警规则表失败:" << query.lastError().text();
        // 规则表创建失败不是致命错误
    }
    
    qInfo() << "[AlarmDatabase] 数据库表创建成功";
    return true;
}

bool AlarmDatabase::insertAlarmEvent(const AlarmEvent& event) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        qWarning() << "[AlarmDatabase] 数据库未初始化";
        return false;
    }
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO alarm_events (
            id, rule_id, rule_name, alarm_type, priority, state,
            channel_name, tag_name, trigger_value, message,
            active_time, acknowledged_time, cleared_time, acknowledged_by,
            has_recording, recording_id, recording_file_path
        ) VALUES (
            :id, :rule_id, :rule_name, :alarm_type, :priority, :state,
            :channel_name, :tag_name, :trigger_value, :message,
            :active_time, :acknowledged_time, :cleared_time, :acknowledged_by,
            :has_recording, :recording_id, :recording_file_path
        )
    )");
    
    query.bindValue(":id", event.id);
    query.bindValue(":rule_id", event.ruleId);
    query.bindValue(":rule_name", event.ruleName);
    query.bindValue(":alarm_type", static_cast<int>(event.type));
    query.bindValue(":priority", static_cast<int>(event.priority));
    query.bindValue(":state", static_cast<int>(event.state));
    query.bindValue(":channel_name", event.channelName);
    query.bindValue(":tag_name", event.tagName);
    query.bindValue(":trigger_value", event.value.toString());
    query.bindValue(":message", event.message);
    query.bindValue(":active_time", event.activeTime.toString(Qt::ISODate));
    query.bindValue(":acknowledged_time", event.acknowledgedTime.isValid() ? 
                    event.acknowledgedTime.toString(Qt::ISODate) : QVariant());
    query.bindValue(":cleared_time", event.clearedTime.isValid() ? 
                    event.clearedTime.toString(Qt::ISODate) : QVariant());
    query.bindValue(":acknowledged_by", event.acknowledgedBy);
    query.bindValue(":has_recording", event.hasRecording ? 1 : 0);
    query.bindValue(":recording_id", event.recordingId);
    query.bindValue(":recording_file_path", event.recordingFilePath);
    
    if (!query.exec()) {
        qWarning() << "[AlarmDatabase] 插入告警事件失败:" << query.lastError().text();
        emit databaseError(tr("插入告警事件失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    return true;
}

bool AlarmDatabase::updateAlarmEvent(const AlarmEvent& event) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        qWarning() << "[AlarmDatabase] 数据库未初始化";
        return false;
    }
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        UPDATE alarm_events SET
            state = :state,
            acknowledged_time = :acknowledged_time,
            cleared_time = :cleared_time,
            acknowledged_by = :acknowledged_by,
            has_recording = :has_recording,
            recording_id = :recording_id,
            recording_file_path = :recording_file_path
        WHERE id = :id
    )");
    
    query.bindValue(":id", event.id);
    query.bindValue(":state", static_cast<int>(event.state));
    query.bindValue(":acknowledged_time", event.acknowledgedTime.isValid() ? 
                    event.acknowledgedTime.toString(Qt::ISODate) : QVariant());
    query.bindValue(":cleared_time", event.clearedTime.isValid() ? 
                    event.clearedTime.toString(Qt::ISODate) : QVariant());
    query.bindValue(":acknowledged_by", event.acknowledgedBy);
    query.bindValue(":has_recording", event.hasRecording ? 1 : 0);
    query.bindValue(":recording_id", event.recordingId);
    query.bindValue(":recording_file_path", event.recordingFilePath);
    
    if (!query.exec()) {
        qWarning() << "[AlarmDatabase] 更新告警事件失败:" << query.lastError().text();
        emit databaseError(tr("更新告警事件失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    return true;
}

QList<AlarmEvent> AlarmDatabase::getAlarmHistory(const QDateTime& start, const QDateTime& end, int limit) {
    QMutexLocker locker(&m_mutex);
    QList<AlarmEvent> result;
    
    if (!m_initialized) {
        qWarning() << "[AlarmDatabase] 数据库未初始化";
        return result;
    }
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT * FROM alarm_events 
        WHERE active_time >= :start AND active_time <= :end
        ORDER BY active_time DESC
        LIMIT :limit
    )");
    
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    query.bindValue(":limit", limit);
    
    if (!query.exec()) {
        qWarning() << "[AlarmDatabase] 查询告警历史失败:" << query.lastError().text();
        return result;
    }
    
    while (query.next()) {
        result.append(eventFromQuery(query));
    }
    
    return result;
}

QList<AlarmEvent> AlarmDatabase::getAlarmsByChannel(const QString& channelName, int limit) {
    QMutexLocker locker(&m_mutex);
    QList<AlarmEvent> result;
    
    if (!m_initialized) {
        return result;
    }
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT * FROM alarm_events 
        WHERE channel_name = :channel_name
        ORDER BY active_time DESC
        LIMIT :limit
    )");
    
    query.bindValue(":channel_name", channelName);
    query.bindValue(":limit", limit);
    
    if (!query.exec()) {
        qWarning() << "[AlarmDatabase] 按通道查询告警失败:" << query.lastError().text();
        return result;
    }
    
    while (query.next()) {
        result.append(eventFromQuery(query));
    }
    
    return result;
}

AlarmEvent AlarmDatabase::getAlarmEvent(const QString& eventId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return AlarmEvent();
    }
    
    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM alarm_events WHERE id = :id");
    query.bindValue(":id", eventId);
    
    if (!query.exec() || !query.next()) {
        return AlarmEvent();
    }
    
    return eventFromQuery(query);
}

int AlarmDatabase::getTotalAlarmCount() {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return 0;
    }
    
    QSqlQuery query(m_db);
    if (!query.exec("SELECT COUNT(*) FROM alarm_events")) {
        return 0;
    }
    
    if (query.next()) {
        return query.value(0).toInt();
    }
    
    return 0;
}

int AlarmDatabase::getAlarmCountInRange(const QDateTime& start, const QDateTime& end) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return 0;
    }
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT COUNT(*) FROM alarm_events 
        WHERE active_time >= :start AND active_time <= :end
    )");
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    
    if (!query.exec() || !query.next()) {
        return 0;
    }
    
    return query.value(0).toInt();
}

QList<AlarmEvent> AlarmDatabase::getAlarmsWithRecording(int limit) {
    QMutexLocker locker(&m_mutex);
    QList<AlarmEvent> result;
    
    if (!m_initialized) {
        return result;
    }
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT * FROM alarm_events 
        WHERE has_recording = 1
        ORDER BY active_time DESC
        LIMIT :limit
    )");
    query.bindValue(":limit", limit);
    
    if (!query.exec()) {
        qWarning() << "[AlarmDatabase] 查询有录波的告警失败:" << query.lastError().text();
        return result;
    }
    
    while (query.next()) {
        result.append(eventFromQuery(query));
    }
    
    return result;
}

bool AlarmDatabase::saveAlarmRule(const AlarmRule& rule) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return false;
    }
    
    // 先尝试更新，如果不存在则插入
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO alarm_rules (
            id, name, enabled, channel_name, tag_name, alarm_type, priority,
            high_limit, low_limit, high_high_limit, low_low_limit,
            deadband, delay_seconds, message, recording_config, updated_at
        ) VALUES (
            :id, :name, :enabled, :channel_name, :tag_name, :alarm_type, :priority,
            :high_limit, :low_limit, :high_high_limit, :low_low_limit,
            :deadband, :delay_seconds, :message, :recording_config, :updated_at
        )
    )");
    
    query.bindValue(":id", rule.id);
    query.bindValue(":name", rule.name);
    query.bindValue(":enabled", rule.enabled ? 1 : 0);
    query.bindValue(":channel_name", rule.channelName);
    query.bindValue(":tag_name", rule.tagName);
    query.bindValue(":alarm_type", static_cast<int>(rule.type));
    query.bindValue(":priority", static_cast<int>(rule.priority));
    query.bindValue(":high_limit", rule.highLimit);
    query.bindValue(":low_limit", rule.lowLimit);
    query.bindValue(":high_high_limit", rule.highHighLimit);
    query.bindValue(":low_low_limit", rule.lowLowLimit);
    query.bindValue(":deadband", rule.deadband);
    query.bindValue(":delay_seconds", rule.delaySeconds);
    query.bindValue(":message", rule.message);
    
    // 将录波配置序列化为JSON
    QJsonDocument doc(rule.recordingConfig.toJson());
    query.bindValue(":recording_config", QString(doc.toJson(QJsonDocument::Compact)));
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString(Qt::ISODate));
    
    if (!query.exec()) {
        qWarning() << "[AlarmDatabase] 保存告警规则失败:" << query.lastError().text();
        return false;
    }
    
    return true;
}

bool AlarmDatabase::deleteAlarmRule(const QString& ruleId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return false;
    }
    
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM alarm_rules WHERE id = :id");
    query.bindValue(":id", ruleId);
    
    if (!query.exec()) {
        qWarning() << "[AlarmDatabase] 删除告警规则失败:" << query.lastError().text();
        return false;
    }
    
    return true;
}

QList<AlarmRule> AlarmDatabase::loadAllRules() {
    QMutexLocker locker(&m_mutex);
    QList<AlarmRule> result;
    
    if (!m_initialized) {
        return result;
    }
    
    QSqlQuery query(m_db);
    if (!query.exec("SELECT * FROM alarm_rules")) {
        qWarning() << "[AlarmDatabase] 加载告警规则失败:" << query.lastError().text();
        return result;
    }
    
    while (query.next()) {
        result.append(ruleFromQuery(query));
    }
    
    return result;
}

int AlarmDatabase::cleanupOldEvents(int keepDays) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return 0;
    }
    
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-keepDays);
    
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM alarm_events WHERE active_time < :cutoff");
    query.bindValue(":cutoff", cutoff.toString(Qt::ISODate));
    
    if (!query.exec()) {
        qWarning() << "[AlarmDatabase] 清理过期告警失败:" << query.lastError().text();
        return 0;
    }
    
    int deleted = query.numRowsAffected();
    if (deleted > 0) {
        qInfo() << "[AlarmDatabase] 清理了" << deleted << "条过期告警记录";
    }
    
    return deleted;
}

bool AlarmDatabase::clearAllHistory() {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return false;
    }
    
    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM alarm_events")) {
        qWarning() << "[AlarmDatabase] 清空告警历史失败:" << query.lastError().text();
        return false;
    }
    
    qInfo() << "[AlarmDatabase] 告警历史已清空";
    return true;
}

AlarmEvent AlarmDatabase::eventFromQuery(QSqlQuery& query) {
    AlarmEvent event;
    
    event.id = query.value("id").toString();
    event.ruleId = query.value("rule_id").toString();
    event.ruleName = query.value("rule_name").toString();
    event.type = static_cast<AlarmType>(query.value("alarm_type").toInt());
    event.priority = static_cast<AlarmPriority>(query.value("priority").toInt());
    event.state = static_cast<AlarmState>(query.value("state").toInt());
    event.channelName = query.value("channel_name").toString();
    event.tagName = query.value("tag_name").toString();
    event.value = query.value("trigger_value");
    event.message = query.value("message").toString();
    event.activeTime = QDateTime::fromString(query.value("active_time").toString(), Qt::ISODate);
    
    QString ackTime = query.value("acknowledged_time").toString();
    if (!ackTime.isEmpty()) {
        event.acknowledgedTime = QDateTime::fromString(ackTime, Qt::ISODate);
    }
    
    QString clrTime = query.value("cleared_time").toString();
    if (!clrTime.isEmpty()) {
        event.clearedTime = QDateTime::fromString(clrTime, Qt::ISODate);
    }
    
    event.acknowledgedBy = query.value("acknowledged_by").toString();
    event.hasRecording = query.value("has_recording").toInt() == 1;
    event.recordingId = query.value("recording_id").toString();
    event.recordingFilePath = query.value("recording_file_path").toString();
    
    return event;
}

AlarmRule AlarmDatabase::ruleFromQuery(QSqlQuery& query) {
    AlarmRule rule;
    
    rule.id = query.value("id").toString();
    rule.name = query.value("name").toString();
    rule.enabled = query.value("enabled").toInt() == 1;
    rule.channelName = query.value("channel_name").toString();
    rule.tagName = query.value("tag_name").toString();
    rule.type = static_cast<AlarmType>(query.value("alarm_type").toInt());
    rule.priority = static_cast<AlarmPriority>(query.value("priority").toInt());
    rule.highLimit = query.value("high_limit").toDouble();
    rule.lowLimit = query.value("low_limit").toDouble();
    rule.highHighLimit = query.value("high_high_limit").toDouble();
    rule.lowLowLimit = query.value("low_low_limit").toDouble();
    rule.deadband = query.value("deadband").toDouble();
    rule.delaySeconds = query.value("delay_seconds").toInt();
    rule.message = query.value("message").toString();
    
    // 解析录波配置JSON
    QString recordingConfigStr = query.value("recording_config").toString();
    if (!recordingConfigStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(recordingConfigStr.toUtf8());
        if (doc.isObject()) {
            rule.recordingConfig = AlarmRecordingConfig::fromJson(doc.object());
        }
    }
    
    return rule;
}

} // namespace ModbusPlexLink
