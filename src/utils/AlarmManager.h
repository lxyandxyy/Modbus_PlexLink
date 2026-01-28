#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QMutex>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QMap>
#include <QVector>
#include "core/DataPoint.h"

namespace ModbusPlexLink {

class ChannelManager;
class Channel;

/**
 * @brief 报警类型枚举
 */
enum class AlarmType {
    HighLimit,      // 高限报警
    LowLimit,       // 低限报警
    HighHighLimit,  // 高高限报警
    LowLowLimit,    // 低低限报警
    ValueChange,    // 值变化报警
    DataQuality,    // 数据质量报警
    ConnectionLost, // 连接丢失报警
    Custom          // 自定义报警
};

/**
 * @brief 报警优先级
 */
enum class AlarmPriority {
    Low = 0,
    Medium = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief 报警状态
 */
enum class AlarmState {
    Active,      // 激活
    Acknowledged,// 已确认
    Cleared      // 已清除
};

/**
 * @brief 录波配置（用于告警自动录波）
 */
struct AlarmRecordingConfig {
    bool enabled;                    // 是否启用告警录波
    double preTriggerSeconds;        // 预触发时间（秒），保留故障前数据
    double postTriggerSeconds;       // 后触发时间（秒），故障后继续录波时间
    int sampleIntervalMs;            // 采样间隔（毫秒）
    QStringList recordTags;          // 要录波的标签列表（空表示使用规则的tagName）
    QString saveDirectory;           // 保存目录（空表示使用默认目录）
    bool autoExportCsv;              // 自动导出CSV
    bool keepInMemory;               // 保留在内存中供回放
    
    AlarmRecordingConfig()
        : enabled(false), preTriggerSeconds(60.0), postTriggerSeconds(30.0),
          sampleIntervalMs(100), autoExportCsv(true), keepInMemory(true) {}
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["enabled"] = enabled;
        obj["preTriggerSeconds"] = preTriggerSeconds;
        obj["postTriggerSeconds"] = postTriggerSeconds;
        obj["sampleIntervalMs"] = sampleIntervalMs;
        obj["recordTags"] = QJsonArray::fromStringList(recordTags);
        obj["saveDirectory"] = saveDirectory;
        obj["autoExportCsv"] = autoExportCsv;
        obj["keepInMemory"] = keepInMemory;
        return obj;
    }
    
    static AlarmRecordingConfig fromJson(const QJsonObject& json) {
        AlarmRecordingConfig config;
        config.enabled = json["enabled"].toBool();
        config.preTriggerSeconds = json["preTriggerSeconds"].toDouble(60.0);
        config.postTriggerSeconds = json["postTriggerSeconds"].toDouble(30.0);
        config.sampleIntervalMs = json["sampleIntervalMs"].toInt(100);
        QJsonArray tagsArray = json["recordTags"].toArray();
        for (const auto& tag : tagsArray) {
            config.recordTags.append(tag.toString());
        }
        config.saveDirectory = json["saveDirectory"].toString();
        config.autoExportCsv = json["autoExportCsv"].toBool(true);
        config.keepInMemory = json["keepInMemory"].toBool(true);
        return config;
    }
};

/**
 * @brief 报警规则
 */
struct AlarmRule {
    QString id;                  // 规则ID
    QString name;                // 规则名称
    bool enabled;                // 是否启用
    QString channelName;         // 通道名称
    QString tagName;             // 标签名称
    AlarmType type;              // 报警类型
    AlarmPriority priority;      // 优先级

    // 阈值
    double highLimit;            // 高限
    double lowLimit;             // 低限
    double highHighLimit;        // 高高限
    double lowLowLimit;          // 低低限
    double deadband;             // 死区

    // 延时
    int delaySeconds;            // 延时秒数（避免误报）

    // 消息
    QString message;             // 报警消息模板
    
    // 录波配置
    AlarmRecordingConfig recordingConfig;  // 告警录波配置

    AlarmRule()
        : enabled(true), type(AlarmType::HighLimit), priority(AlarmPriority::Medium),
          highLimit(100.0), lowLimit(0.0), highHighLimit(120.0), lowLowLimit(-10.0),
          deadband(1.0), delaySeconds(0) {}

    // 序列化
    QJsonObject toJson() const;
    static AlarmRule fromJson(const QJsonObject& json);
};

/**
 * @brief 录波数据记录
 */
struct AlarmRecordingData {
    QString alarmEventId;            // 关联的报警事件ID
    QDateTime startTime;             // 录波开始时间
    QDateTime endTime;               // 录波结束时间
    QString csvFilePath;             // CSV文件路径
    QStringList recordedTags;        // 录波的标签列表
    int totalDataPoints;             // 总数据点数
    bool isComplete;                 // 是否完成
    
    // 内存中的录波数据（用于回放）
    struct DataPoint {
        qint64 timestamp;            // 毫秒时间戳
        QMap<QString, double> values;// 标签名 -> 值
    };
    QVector<DataPoint> data;
    
    AlarmRecordingData() : totalDataPoints(0), isComplete(false) {}
};

/**
 * @brief 报警事件
 */
struct AlarmEvent {
    QString id;                  // 事件ID（UUID）
    QString ruleId;              // 规则ID
    QString ruleName;            // 规则名称
    AlarmType type;              // 报警类型
    AlarmPriority priority;      // 优先级
    AlarmState state;            // 状态

    QString channelName;         // 通道名称
    QString tagName;             // 标签名称
    QVariant value;              // 触发值
    QString message;             // 报警消息

    QDateTime activeTime;        // 激活时间
    QDateTime acknowledgedTime;  // 确认时间
    QDateTime clearedTime;       // 清除时间
    QString acknowledgedBy;      // 确认人
    
    // 录波数据关联
    bool hasRecording;           // 是否有录波数据
    QString recordingId;         // 录波数据ID
    QString recordingFilePath;   // 录波文件路径

    AlarmEvent()
        : type(AlarmType::HighLimit), priority(AlarmPriority::Medium),
          state(AlarmState::Active), hasRecording(false) {}

    // 序列化
    QJsonObject toJson() const;
    static AlarmEvent fromJson(const QJsonObject& json);
};

/**
 * @brief 报警管理器
 *
 * 功能：
 * - 报警规则管理（增删改查）
 * - 实时监控数据点，触发报警
 * - 报警事件记录
 * - 报警确认与清除
 * - 报警历史查询
 * - 报警统计
 */
class AlarmManager : public QObject {
    Q_OBJECT

public:
    explicit AlarmManager(ChannelManager* channelManager, QObject *parent = nullptr);
    ~AlarmManager();

    // 规则管理
    QString addRule(const AlarmRule& rule);
    bool updateRule(const QString& ruleId, const AlarmRule& rule);
    bool removeRule(const QString& ruleId);
    AlarmRule getRule(const QString& ruleId) const;
    QList<AlarmRule> getAllRules() const;
    void clearAllRules();

    // 规则启用/禁用
    bool enableRule(const QString& ruleId, bool enable);

    // 事件管理
    QList<AlarmEvent> getActiveAlarms() const;
    QList<AlarmEvent> getAlarmHistory(const QDateTime& start, const QDateTime& end) const;
    AlarmEvent getAlarmEvent(const QString& eventId) const;

    // 报警确认
    bool acknowledgeAlarm(const QString& eventId, const QString& acknowledgedBy = QString());

    // 报警清除（手动）
    bool clearAlarm(const QString& eventId);

    // 统计
    int getActiveAlarmCount() const;
    int getAlarmCount(AlarmPriority priority) const;
    int getTotalAlarmCount() const;

    // 配置
    bool loadConfig(const QString& filename);
    bool saveConfig(const QString& filename) const;
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
    
    // ==================== 录波相关 ====================
    
    /**
     * @brief 设置默认录波保存目录
     */
    void setRecordingSaveDirectory(const QString& directory);
    QString recordingSaveDirectory() const { return m_recordingSaveDir; }
    
    /**
     * @brief 获取报警事件的录波数据
     */
    AlarmRecordingData getRecordingData(const QString& alarmEventId) const;
    
    /**
     * @brief 获取所有有录波数据的报警事件
     */
    QList<AlarmEvent> getAlarmsWithRecording() const;
    
    /**
     * @brief 手动导出录波数据到CSV
     */
    bool exportRecordingToCsv(const QString& alarmEventId, const QString& filename);
    
    /**
     * @brief 清理过期的录波数据（释放内存）
     */
    void cleanupOldRecordings(int keepDays = 7);

signals:
    // 新报警触发
    void alarmTriggered(const AlarmEvent& event);

    // 报警确认
    void alarmAcknowledged(const QString& eventId);

    // 报警清除
    void alarmCleared(const QString& eventId);

    // 规则变化
    void ruleAdded(const QString& ruleId);
    void ruleUpdated(const QString& ruleId);
    void ruleRemoved(const QString& ruleId);
    
    // 录波相关信号
    void recordingStarted(const QString& alarmEventId);
    void recordingProgress(const QString& alarmEventId, int dataPoints, double elapsedSeconds);
    void recordingCompleted(const QString& alarmEventId, const QString& csvFilePath);
    void recordingDataAvailable(const QString& alarmEventId);

private slots:
    // 数据更新监听
    void onDataUpdated(const QString& tagName, const DataPoint& point);

    // 连接状态监听
    void onCollectorConnectionChanged(const QString& collectorName, bool connected);

private:
    // 检查报警条件
    void checkAlarmConditions(const QString& channelName, const QString& tagName, const DataPoint& point);

    // 触发报警
    void triggerAlarm(const AlarmRule& rule, const QString& channelName,
                      const QString& tagName, const QVariant& value);

    // 自动清除报警
    void autoClearAlarm(const QString& ruleId);

    // 生成报警消息
    QString generateAlarmMessage(const AlarmRule& rule, const QVariant& value) const;

    // 连接通道信号
    void connectChannelSignals(Channel* channel);

private slots:
    // 录波定时器
    void onRecordingTimer();
    void onPostTriggerTimeout();
    void onPreTriggerSampleTimer();  // 预触发缓冲区持续采样

private:
    ChannelManager* m_channelManager;

    // 规则
    QList<AlarmRule> m_rules;
    mutable QMutex m_rulesMutex;

    // 活动报警
    QList<AlarmEvent> m_activeAlarms;
    QMap<QString, AlarmEvent> m_activeAlarmsMap;  // ruleId -> 最新活动报警

    // 历史报警（最近1000条）
    QList<AlarmEvent> m_alarmHistory;
    int m_maxHistorySize;

    mutable QMutex m_alarmsMutex;

    // 延时检查缓存（避免瞬时抖动）
    struct DelayEntry {
        QDateTime firstTriggerTime;
        QVariant value;
    };
    QMap<QString, DelayEntry> m_delayCache;  // ruleId -> DelayEntry
    
    // ==================== 录波相关 ====================
    
    // 预触发缓冲区（环形缓冲，按规则ID存储）
    struct PreTriggerBuffer {
        AlarmRecordingConfig config;
        QVector<AlarmRecordingData::DataPoint> buffer;
        int maxSize;
        QTimer* sampleTimer;
        QStringList tags;
    };
    QMap<QString, PreTriggerBuffer> m_preTriggerBuffers;  // ruleId -> buffer
    
    // 活动录波会话
    struct RecordingSession {
        QString alarmEventId;
        QString ruleId;
        AlarmRecordingConfig config;
        QVector<AlarmRecordingData::DataPoint> data;
        QDateTime startTime;
        QTimer* postTriggerTimer;
        bool isActive;
    };
    QMap<QString, RecordingSession> m_activeSessions;  // alarmEventId -> session
    
    // 已完成的录波数据
    QMap<QString, AlarmRecordingData> m_completedRecordings;  // alarmEventId -> data
    
    // 录波采样定时器
    QTimer* m_recordingSampleTimer;
    
    // 预触发缓冲区采样定时器（持续运行）
    QTimer* m_preTriggerSampleTimer;
    
    // 默认保存目录
    QString m_recordingSaveDir;
    
    // 录波相关方法
    void startRecordingForAlarm(AlarmEvent& event, const AlarmRule& rule);
    void stopRecordingForAlarm(const QString& alarmEventId);
    void sampleRecordingData();
    void flushPreTriggerBuffer(const QString& ruleId, RecordingSession& session);
    void saveRecordingToCsv(const QString& alarmEventId);
    void initPreTriggerBuffer(const AlarmRule& rule);
    void updatePreTriggerBuffer(const QString& ruleId);
    void startPreTriggerSampling();  // 启动预触发采样
    void stopPreTriggerSampling();   // 停止预触发采样
};

} // namespace ModbusPlexLink

#endif // ALARMMANAGER_H
