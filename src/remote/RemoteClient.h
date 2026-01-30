#ifndef REMOTECLIENT_H
#define REMOTECLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QUrl>

namespace ModbusPlexLink {

/**
 * @brief 远程API客户端
 * 
 * 用于从远程GUI连接到边缘网关服务
 * 提供与ChannelManager类似的接口，但通过网络调用
 */
class RemoteClient : public QObject {
    Q_OBJECT
    
public:
    enum ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Error
    };
    Q_ENUM(ConnectionState)
    
    explicit RemoteClient(QObject* parent = nullptr);
    ~RemoteClient();
    
    // 连接管理
    void connectToHost(const QString& host, quint16 httpPort = 8080, quint16 wsPort = 8081);
    void disconnect();
    bool isConnected() const;
    ConnectionState getState() const { return m_state; }
    QString getLastError() const { return m_lastError; }
    
    // 认证设置
    void setCredentials(const QString& username, const QString& password);
    
    // ========== 通道管理 API ==========
    
    // 获取所有通道
    void getChannels();
    
    // 获取所有可用标签（用于录波添加通道）
    void getAllTags();
    
    // 获取单个通道详情
    void getChannel(const QString& name);
    
    // 创建通道
    // 创建通道（简单模式：只传名称）
    void createChannel(const QString& name);
    
    // 创建通道（完整模式：传完整配置）
    void createChannel(const QJsonObject& config);
    
    // 更新通道配置
    void updateChannel(const QString& name, const QJsonObject& config);
    
    // 删除通道
    void deleteChannel(const QString& name);
    
    // 启动通道
    void startChannel(const QString& name);
    
    // 停止通道
    void stopChannel(const QString& name);
    
    // ========== 采集器管理 API ==========
    
    void getCollectors(const QString& channelName);
    void addCollector(const QString& channelName, const QJsonObject& config);
    void removeCollector(const QString& channelName, const QString& collectorName);
    
    // ========== 服务器管理 API ==========
    
    void getServers(const QString& channelName);
    void addServer(const QString& channelName, const QJsonObject& config);
    
    // ========== 数据访问 API ==========
    
    void getData(const QString& channelName);
    void getDataPoint(const QString& channelName, const QString& tagName);
    void writeDataPoint(const QString& channelName, const QString& tagName, const QVariant& value);
    
    // ========== 配置管理 API ==========
    
    void getConfig();
    void setConfig(const QJsonObject& config);
    void saveConfig();
    void loadConfig(const QString& filename);
    
    // ========== 系统信息 API ==========
    
    void getSystemInfo();
    void getStatistics();
    
    // ========== 报警管理 API ==========
    
    void getAlarms();
    void getAlarmHistory(int days = 7, int limit = 1000);
    void acknowledgeAlarm(const QString& alarmId);
    void clearAlarm(const QString& alarmId);
    
    // 告警规则管理
    void getAlarmRules();
    void addAlarmRule(const QJsonObject& rule);
    void updateAlarmRule(const QString& ruleId, const QJsonObject& rule);
    void deleteAlarmRule(const QString& ruleId);
    void enableAlarmRule(const QString& ruleId, bool enable);
    
    // 告警录波
    void getAlarmRecording(const QString& alarmId);
    void getAlarmRecordings();
    
    // ========== WebSocket订阅 ==========
    
    void subscribeToData();
    void subscribeToMessages();
    void subscribeToLogs();
    void subscribeToAlarms();
    void subscribeToStatus();
    void subscribeToChannel();  // 订阅通道状态变化
    void subscribeToMessage();  // 订阅Modbus报文
    void unsubscribe(const QString& topic);
    
signals:
    // 连接状态
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void stateChanged(ConnectionState state);
    
    // API响应
    void channelsReceived(const QJsonArray& channels);
    void allTagsReceived(const QJsonArray& tags);
    void channelReceived(const QJsonObject& channel);
    void channelCreated(const QString& name, bool success);
    void channelUpdated(const QString& name, bool success);
    void channelDeleted(const QString& name, bool success);
    void channelStarted(const QString& name, bool success);
    void channelStopped(const QString& name, bool success);
    
    void collectorsReceived(const QString& channelName, const QJsonArray& collectors);
    void collectorAdded(const QString& channelName, bool success);
    void collectorRemoved(const QString& channelName, const QString& collectorName, bool success);
    
    void serversReceived(const QString& channelName, const QJsonArray& servers);
    void serverAdded(const QString& channelName, bool success);
    
    void dataReceived(const QString& channelName, const QJsonObject& data);
    void dataPointReceived(const QString& channelName, const QString& tagName, const QJsonObject& point);
    void dataPointWritten(const QString& channelName, const QString& tagName, bool success);
    
    void configReceived(const QJsonObject& config);
    void configSet(bool success);
    void configSaved(bool success);
    void configLoaded(const QString& filename, bool success);
    
    void systemInfoReceived(const QJsonObject& info);
    void statisticsReceived(const QJsonObject& statistics);
    
    void alarmsReceived(const QJsonArray& alarms);
    void alarmHistoryReceived(const QJsonArray& alarms, int days);
    void alarmAcknowledged(const QString& alarmId, bool success);
    void alarmCleared(const QString& alarmId, bool success);
    
    // 告警规则信号
    void alarmRulesReceived(const QJsonArray& rules);
    void alarmRuleAdded(const QString& ruleId, bool success);
    void alarmRuleUpdated(const QString& ruleId, bool success);
    void alarmRuleDeleted(const QString& ruleId, bool success);
    void alarmRuleEnabled(const QString& ruleId, bool enabled, bool success);
    
    // 告警录波信号
    void alarmRecordingReceived(const QString& alarmId, const QJsonObject& recording);
    void alarmRecordingsReceived(const QJsonArray& recordings);
    
    // 实时推送
    void realtimeDataReceived(const QString& channelName, const QJsonObject& data);
    void realtimeMessageReceived(const QJsonObject& message);
    void realtimeChannelEventReceived(const QJsonObject& event);  // 通道状态变化事件
    void realtimeLogReceived(const QJsonObject& log);
    void realtimeAlarmReceived(const QJsonObject& alarm);
    void realtimeStatusReceived(const QJsonObject& status);
    
    // 错误
    void apiError(const QString& endpoint, const QString& error);
    
private slots:
    void onHttpFinished(QNetworkReply* reply);
    void onWsConnected();
    void onWsDisconnected();
    void onWsTextMessageReceived(const QString& message);
    void onWsError(QAbstractSocket::SocketError error);
    void onReconnectTimer();
    
private:
    // HTTP请求
    void get(const QString& endpoint);
    void post(const QString& endpoint, const QJsonObject& body = QJsonObject());
    void put(const QString& endpoint, const QJsonObject& body);
    void del(const QString& endpoint);
    
    QNetworkRequest createRequest(const QString& endpoint);
    void handleResponse(QNetworkReply* reply, const QString& endpoint);
    
    // 状态管理
    void setState(ConnectionState state);
    void setError(const QString& error);
    
    // WebSocket消息
    void sendWsMessage(const QString& type, const QJsonObject& data = QJsonObject());
    
    // 成员变量
    QNetworkAccessManager* m_networkManager;
    QWebSocket* m_webSocket;
    QTimer* m_reconnectTimer;
    QTimer* m_pingTimer;
    
    QString m_host;
    quint16 m_httpPort;
    quint16 m_wsPort;
    QString m_username;
    QString m_password;
    
    ConnectionState m_state;
    QString m_lastError;
    bool m_autoReconnect;
    int m_reconnectInterval;
    
    // 跟踪待处理的请求 (value = "METHOD:endpoint")
    QMap<QNetworkReply*, QString> m_pendingRequests;
};

} // namespace ModbusPlexLink

#endif // REMOTECLIENT_H
