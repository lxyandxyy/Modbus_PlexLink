#ifndef ALARMDATABASE_H
#define ALARMDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMutex>
#include <QDateTime>
#include "AlarmManager.h"

namespace ModbusPlexLink {

/**
 * @brief 告警数据库管理类
 * 
 * 使用SQLite存储历史告警数据，解决重启后数据丢失问题
 * 单例模式，线程安全
 */
class AlarmDatabase : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief 获取单例实例
     */
    static AlarmDatabase& instance();
    
    /**
     * @brief 初始化数据库
     * @param dbPath 数据库文件路径，默认为 "alarms.db"
     * @return 是否成功
     */
    bool initialize(const QString& dbPath = "alarms.db");
    
    /**
     * @brief 关闭数据库连接
     */
    void close();
    
    /**
     * @brief 检查数据库是否已初始化
     */
    bool isInitialized() const { return m_initialized; }
    
    // ==================== 告警事件操作 ====================
    
    /**
     * @brief 插入告警事件
     * @param event 告警事件
     * @return 是否成功
     */
    bool insertAlarmEvent(const AlarmEvent& event);
    
    /**
     * @brief 更新告警事件（确认、清除等）
     * @param event 告警事件
     * @return 是否成功
     */
    bool updateAlarmEvent(const AlarmEvent& event);
    
    /**
     * @brief 获取告警历史
     * @param start 开始时间
     * @param end 结束时间
     * @param limit 最大返回数量，默认1000
     * @return 告警事件列表
     */
    QList<AlarmEvent> getAlarmHistory(const QDateTime& start, const QDateTime& end, int limit = 1000);
    
    /**
     * @brief 按通道获取告警历史
     * @param channelName 通道名称
     * @param limit 最大返回数量
     * @return 告警事件列表
     */
    QList<AlarmEvent> getAlarmsByChannel(const QString& channelName, int limit = 100);
    
    /**
     * @brief 获取单个告警事件
     * @param eventId 事件ID
     * @return 告警事件
     */
    AlarmEvent getAlarmEvent(const QString& eventId);
    
    /**
     * @brief 获取总告警数量
     * @return 总数
     */
    int getTotalAlarmCount();
    
    /**
     * @brief 获取指定时间范围内的告警数量
     */
    int getAlarmCountInRange(const QDateTime& start, const QDateTime& end);
    
    /**
     * @brief 获取有录波数据的告警
     * @param limit 最大返回数量
     * @return 告警事件列表
     */
    QList<AlarmEvent> getAlarmsWithRecording(int limit = 100);
    
    // ==================== 告警规则操作（可选） ====================
    
    /**
     * @brief 保存告警规则
     * @param rule 告警规则
     * @return 是否成功
     */
    bool saveAlarmRule(const AlarmRule& rule);
    
    /**
     * @brief 删除告警规则
     * @param ruleId 规则ID
     * @return 是否成功
     */
    bool deleteAlarmRule(const QString& ruleId);
    
    /**
     * @brief 加载所有告警规则
     * @return 告警规则列表
     */
    QList<AlarmRule> loadAllRules();
    
    // ==================== 数据清理 ====================
    
    /**
     * @brief 清理过期的告警事件
     * @param keepDays 保留天数，默认90天
     * @return 删除的记录数
     */
    int cleanupOldEvents(int keepDays = 90);
    
    /**
     * @brief 清空所有告警历史
     * @return 是否成功
     */
    bool clearAllHistory();
    
signals:
    /**
     * @brief 数据库错误信号
     * @param error 错误信息
     */
    void databaseError(const QString& error);
    
private:
    AlarmDatabase();
    ~AlarmDatabase();
    
    // 禁止拷贝
    AlarmDatabase(const AlarmDatabase&) = delete;
    AlarmDatabase& operator=(const AlarmDatabase&) = delete;
    
    /**
     * @brief 创建数据库表
     * @return 是否成功
     */
    bool createTables();
    
    /**
     * @brief 从查询结果构建告警事件
     * @param query SQL查询对象
     * @return 告警事件
     */
    AlarmEvent eventFromQuery(QSqlQuery& query);
    
    /**
     * @brief 从查询结果构建告警规则
     * @param query SQL查询对象
     * @return 告警规则
     */
    AlarmRule ruleFromQuery(QSqlQuery& query);
    
    QSqlDatabase m_db;
    mutable QMutex m_mutex;
    bool m_initialized;
    QString m_connectionName;
};

} // namespace ModbusPlexLink

#endif // ALARMDATABASE_H
