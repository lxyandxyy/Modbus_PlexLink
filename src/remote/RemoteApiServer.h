#ifndef REMOTEAPISERVER_H
#define REMOTEAPISERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QTimer>
#include <QMutex>
#include "core/Channel.h"

namespace ModbusPlexLink {

class ChannelManager;
class AlarmManager;

/**
 * @brief HTTP请求结构
 */
struct HttpRequest {
    QString method;      // GET, POST, PUT, DELETE
    QString path;        // /api/channels
    QString version;     // HTTP/1.1
    QMap<QString, QString> headers;
    QByteArray body;
    QMap<QString, QString> queryParams;
    QStringList pathParams;  // 路径参数
};

/**
 * @brief HTTP响应结构
 */
struct HttpResponse {
    int statusCode = 200;
    QString statusText = "OK";
    QMap<QString, QString> headers;
    QByteArray body;
    
    void setJson(const QJsonObject& json);
    void setJson(const QJsonArray& json);
    void setError(int code, const QString& message);
    void setSuccess(const QString& message = "OK");
    QByteArray toBytes() const;
};

/**
 * @brief HTTP连接处理器
 */
class HttpConnection : public QObject {
    Q_OBJECT
    
public:
    explicit HttpConnection(QTcpSocket* socket, QObject* parent = nullptr);
    ~HttpConnection();
    
signals:
    void requestReceived(const HttpRequest& request, HttpConnection* connection);
    void disconnected();
    
public slots:
    void sendResponse(const HttpResponse& response);
    
private slots:
    void onReadyRead();
    void onDisconnected();
    
private:
    bool parseRequest(const QByteArray& data, HttpRequest& request);
    void parseQueryString(const QString& queryString, QMap<QString, QString>& params);
    
    QTcpSocket* m_socket;
    QByteArray m_buffer;
};

/**
 * @brief WebSocket客户端包装器
 */
class WebSocketClient : public QObject {
    Q_OBJECT
    
public:
    explicit WebSocketClient(QWebSocket* socket, QObject* parent = nullptr);
    ~WebSocketClient();
    
    void sendMessage(const QJsonObject& message);
    void sendMessage(const QString& type, const QJsonObject& data);
    QString getClientId() const { return m_clientId; }
    
    // 订阅管理
    void subscribe(const QString& topic);
    void unsubscribe(const QString& topic);
    bool isSubscribed(const QString& topic) const;
    QStringList getSubscriptions() const;
    
signals:
    void messageReceived(const QJsonObject& message, WebSocketClient* client);
    void disconnected(WebSocketClient* client);
    
private slots:
    void onTextMessageReceived(const QString& message);
    void onDisconnected();
    
private:
    QWebSocket* m_socket;
    QString m_clientId;
    QStringList m_subscriptions;
};

/**
 * @brief 远程API服务器
 * 
 * 提供HTTP REST API和WebSocket实时推送功能
 * 供远程客户端配置和监控边缘网关
 */
class RemoteApiServer : public QObject {
    Q_OBJECT
    
public:
    explicit RemoteApiServer(ChannelManager* channelManager, 
                            AlarmManager* alarmManager = nullptr,
                            QObject* parent = nullptr);
    ~RemoteApiServer();
    
    // 启动服务器
    bool start(quint16 httpPort = 8080, quint16 wsPort = 8081);
    
    // 停止服务器
    void stop();
    
    // 检查是否运行中
    bool isRunning() const;
    
    // 获取服务器地址
    QString getHttpAddress() const;
    QString getWebSocketAddress() const;
    
    // 设置认证（可选）
    void setAuthentication(bool enabled, const QString& username = "", const QString& password = "");
    
signals:
    void clientConnected(const QString& clientId);
    void clientDisconnected(const QString& clientId);
    void requestReceived(const QString& method, const QString& path);
    void serverError(const QString& error);
    
private slots:
    // HTTP服务器槽
    void onHttpConnection();
    void handleHttpRequest(const HttpRequest& request, HttpConnection* connection);
    
    // WebSocket服务器槽
    void onWsConnection();
    void handleWsMessage(const QJsonObject& message, WebSocketClient* client);
    void onWsClientDisconnected(WebSocketClient* client);
    
    // 数据推送定时器
    void pushRealtimeData();
    
    // 通道管理槽
    void onChannelCreated(const QString& channelName);
    void onChannelDeleted(const QString& channelName);
    void onChannelStateChanged(const QString& channelName, ChannelState state);
    void onChannelModbusMessage(const QString& channelName, const QString& source, 
                                const QString& direction, const QString& device,
                                const QString& function, const QString& address,
                                const QString& data, bool success);
    
private:
    // HTTP API处理函数
    HttpResponse handleGetChannels();
    HttpResponse handleGetChannel(const QString& name);
    HttpResponse handleCreateChannel(const QJsonObject& body);
    HttpResponse handleUpdateChannel(const QString& name, const QJsonObject& body);
    HttpResponse handleDeleteChannel(const QString& name);
    HttpResponse handleStartChannel(const QString& name);
    HttpResponse handleStopChannel(const QString& name);
    
    HttpResponse handleGetCollectors(const QString& channelName);
    HttpResponse handleAddCollector(const QString& channelName, const QJsonObject& body);
    HttpResponse handleUpdateCollector(const QString& channelName, const QString& collectorName, const QJsonObject& body);
    HttpResponse handleDeleteCollector(const QString& channelName, const QString& collectorName);
    
    HttpResponse handleGetServers(const QString& channelName);
    HttpResponse handleAddServer(const QString& channelName, const QJsonObject& body);
    
    HttpResponse handleGetAllTags();
    HttpResponse handleGetData(const QString& channelName);
    HttpResponse handleGetDataPoint(const QString& channelName, const QString& tagName);
    HttpResponse handleWriteDataPoint(const QString& channelName, const QString& tagName, const QJsonObject& body);
    
    HttpResponse handleGetConfig();
    HttpResponse handleSetConfig(const QJsonObject& body);
    HttpResponse handleSaveConfig();
    HttpResponse handleLoadConfig(const QString& filename);
    
    HttpResponse handleGetStatistics();
    
    // 告警管理API
    HttpResponse handleGetAlarms();
    HttpResponse handleGetAlarmHistory(const QMap<QString, QString>& queryParams);
    HttpResponse handleGetAlarmRules();
    HttpResponse handleAddAlarmRule(const QJsonObject& body);
    HttpResponse handleUpdateAlarmRule(const QString& ruleId, const QJsonObject& body);
    HttpResponse handleDeleteAlarmRule(const QString& ruleId);
    HttpResponse handleEnableAlarmRule(const QString& ruleId, bool enable);
    HttpResponse handleAcknowledgeAlarm(const QString& alarmId);
    HttpResponse handleClearAlarm(const QString& alarmId);
    HttpResponse handleGetAlarmRecording(const QString& alarmId);
    HttpResponse handleGetAlarmRecordings();
    
    HttpResponse handleGetSystemInfo();
    
    // 路由匹配
    bool matchRoute(const QString& pattern, const QString& path, QStringList& params);
    
    // 认证检查
    bool checkAuth(const HttpRequest& request);
    
    // WebSocket广播
    void broadcastToSubscribers(const QString& topic, const QJsonObject& data);
    
    // 连接通道信号
    void connectChannelSignals(Channel* channel);
    
    // 成员变量
    ChannelManager* m_channelManager;
    AlarmManager* m_alarmManager;
    
    QTcpServer* m_httpServer;
    QWebSocketServer* m_wsServer;
    
    QList<HttpConnection*> m_httpConnections;
    QList<WebSocketClient*> m_wsClients;
    
    QTimer* m_pushTimer;
    int m_pushInterval = 1000;  // 默认1秒推送一次
    
    bool m_authEnabled = false;
    QString m_authUsername;
    QString m_authPassword;
    
    quint16 m_httpPort = 8080;
    quint16 m_wsPort = 8081;
    
    mutable QMutex m_mutex;
};

} // namespace ModbusPlexLink

#endif // REMOTEAPISERVER_H
