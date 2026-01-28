#include "RemoteClient.h"
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QDebug>

namespace ModbusPlexLink {

RemoteClient::RemoteClient(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_webSocket(new QWebSocket())
    , m_reconnectTimer(new QTimer(this))
    , m_pingTimer(new QTimer(this))
    , m_httpPort(8080)
    , m_wsPort(8081)
    , m_state(Disconnected)
    , m_autoReconnect(true)
    , m_reconnectInterval(5000)
{
    // 禁用代理，避免 "The proxy type is invalid for this operation" 错误
    m_networkManager->setProxy(QNetworkProxy::NoProxy);
    m_webSocket->setProxy(QNetworkProxy::NoProxy);
    
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &RemoteClient::onHttpFinished);
    
    connect(m_webSocket, &QWebSocket::connected,
            this, &RemoteClient::onWsConnected);
    connect(m_webSocket, &QWebSocket::disconnected,
            this, &RemoteClient::onWsDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived,
            this, &RemoteClient::onWsTextMessageReceived);
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &RemoteClient::onWsError);
    
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &RemoteClient::onReconnectTimer);
    
    connect(m_pingTimer, &QTimer::timeout, this, [this]() {
        if (m_state == Connected) {
            sendWsMessage("ping");
        }
    });
}

RemoteClient::~RemoteClient() {
    disconnect();
    delete m_webSocket;
}

void RemoteClient::connectToHost(const QString& host, quint16 httpPort, quint16 wsPort) {
    m_host = host;
    m_httpPort = httpPort;
    m_wsPort = wsPort;
    
    setState(Connecting);
    
    // 首先测试HTTP连接
    qDebug() << "RemoteClient: Connecting to" << host << "HTTP:" << httpPort << "WS:" << wsPort;
    
    // 连接WebSocket
    QUrl wsUrl;
    wsUrl.setScheme("ws");
    wsUrl.setHost(host);
    wsUrl.setPort(wsPort);
    
    m_webSocket->open(wsUrl);
}

void RemoteClient::disconnect() {
    m_reconnectTimer->stop();
    m_pingTimer->stop();
    
    if (m_webSocket->isValid()) {
        m_webSocket->close();
    }
    
    setState(Disconnected);
}

bool RemoteClient::isConnected() const {
    return m_state == Connected;
}

void RemoteClient::setCredentials(const QString& username, const QString& password) {
    m_username = username;
    m_password = password;
}

// ============================================================================
// HTTP请求方法
// ============================================================================

QNetworkRequest RemoteClient::createRequest(const QString& endpoint) {
    QUrl url;
    url.setScheme("http");
    url.setHost(m_host);
    url.setPort(m_httpPort);
    url.setPath(endpoint);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 添加认证
    if (!m_username.isEmpty()) {
        QString credentials = QString("%1:%2").arg(m_username).arg(m_password);
        QByteArray encoded = credentials.toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + encoded);
    }
    
    return request;
}

void RemoteClient::get(const QString& endpoint) {
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply* reply = m_networkManager->get(request);
    m_pendingRequests[reply] = "GET:" + endpoint;
}

void RemoteClient::post(const QString& endpoint, const QJsonObject& body) {
    QNetworkRequest request = createRequest(endpoint);
    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_networkManager->post(request, data);
    m_pendingRequests[reply] = "POST:" + endpoint;
}

void RemoteClient::put(const QString& endpoint, const QJsonObject& body) {
    QNetworkRequest request = createRequest(endpoint);
    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_networkManager->put(request, data);
    m_pendingRequests[reply] = "PUT:" + endpoint;
}

void RemoteClient::del(const QString& endpoint) {
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply* reply = m_networkManager->deleteResource(request);
    m_pendingRequests[reply] = "DELETE:" + endpoint;
}

void RemoteClient::onHttpFinished(QNetworkReply* reply) {
    QString methodAndEndpoint = m_pendingRequests.take(reply);
    handleResponse(reply, methodAndEndpoint);
    reply->deleteLater();
}

void RemoteClient::handleResponse(QNetworkReply* reply, const QString& methodAndEndpoint) {
    // 解析方法和endpoint
    int colonPos = methodAndEndpoint.indexOf(':');
    QString method = colonPos > 0 ? methodAndEndpoint.left(colonPos) : "GET";
    QString endpoint = colonPos > 0 ? methodAndEndpoint.mid(colonPos + 1) : methodAndEndpoint;
    
    if (reply->error() != QNetworkReply::NoError) {
        emit apiError(endpoint, reply->errorString());
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        emit apiError(endpoint, "Invalid JSON response");
        return;
    }
    
    QJsonObject response = doc.object();
    bool success = response["success"].toBool();
    
    // 根据method和endpoint分发响应
    if (endpoint == "/api/channels" && method == "GET") {
        emit channelsReceived(response["channels"].toArray());
    }
    else if (endpoint == "/api/channels" && method == "POST") {
        // 创建通道
        QString name = response["message"].toString();  // 响应中包含通道名
        // 从消息中提取通道名 "Channel created: xxx"
        if (name.contains(":")) {
            name = name.split(":").last().trimmed();
        }
        emit channelCreated(name, success);
    }
    else if (endpoint.startsWith("/api/channels/") && endpoint.endsWith("/start")) {
        // /api/channels/{name}/start
        QStringList parts = endpoint.split('/');
        QString name = parts.value(3);  // 通道名在第4个位置
        emit channelStarted(name, success);
    }
    else if (endpoint.startsWith("/api/channels/") && endpoint.endsWith("/stop")) {
        // /api/channels/{name}/stop
        QStringList parts = endpoint.split('/');
        QString name = parts.value(3);  // 通道名在第4个位置
        emit channelStopped(name, success);
    }
    else if (endpoint.startsWith("/api/channels/") && method == "DELETE") {
        // 删除通道: /api/channels/{name}
        QString name = endpoint.split('/').last();
        emit channelDeleted(name, success);
    }
    else if (endpoint.startsWith("/api/channels/") && method == "PUT" && endpoint.count('/') == 3) {
        // 更新通道: /api/channels/{name}
        QString name = endpoint.split('/').last();
        emit channelUpdated(name, success);
    }
    else if (endpoint.startsWith("/api/channels/") && method == "GET" && endpoint.count('/') == 3) {
        // 获取单个通道: /api/channels/{name}
            emit channelReceived(response["channel"].toObject());
    }
    else if (endpoint.contains("/collectors")) {
        QString channelName = endpoint.split('/')[3];
        emit collectorsReceived(channelName, response["collectors"].toArray());
    }
    else if (endpoint.contains("/servers")) {
        QString channelName = endpoint.split('/')[3];
        emit serversReceived(channelName, response["servers"].toArray());
    }
    else if (endpoint.startsWith("/api/data/")) {
        QStringList parts = endpoint.split('/');
        QString channelName = parts.value(3);
        if (parts.size() > 4) {
            QString tagName = parts.value(4);
            emit dataPointReceived(channelName, tagName, response);
        } else {
            emit dataReceived(channelName, response["data"].toObject());
        }
    }
    else if (endpoint == "/api/config") {
        emit configReceived(response["config"].toObject());
    }
    else if (endpoint == "/api/config/save") {
        emit configSaved(success);
    }
    else if (endpoint == "/api/system") {
        emit systemInfoReceived(response["system"].toObject());
    }
    else if (endpoint == "/api/statistics") {
        emit statisticsReceived(response["statistics"].toObject());
    }
    else if (endpoint == "/api/alarms") {
        emit alarmsReceived(response["alarms"].toArray());
    }
}

// ============================================================================
// 通道管理 API
// ============================================================================

void RemoteClient::getChannels() {
    get("/api/channels");
}

void RemoteClient::getChannel(const QString& name) {
    get(QString("/api/channels/%1").arg(name));
}

void RemoteClient::createChannel(const QString& name) {
    QJsonObject body;
    body["name"] = name;
    post("/api/channels", body);
}

void RemoteClient::createChannel(const QJsonObject& config) {
    post("/api/channels", config);
}

void RemoteClient::updateChannel(const QString& name, const QJsonObject& config) {
    put(QString("/api/channels/%1").arg(name), config);
}

void RemoteClient::deleteChannel(const QString& name) {
    del(QString("/api/channels/%1").arg(name));
}

void RemoteClient::startChannel(const QString& name) {
    post(QString("/api/channels/%1/start").arg(name));
}

void RemoteClient::stopChannel(const QString& name) {
    post(QString("/api/channels/%1/stop").arg(name));
}

// ============================================================================
// 采集器管理 API
// ============================================================================

void RemoteClient::getCollectors(const QString& channelName) {
    get(QString("/api/channels/%1/collectors").arg(channelName));
}

void RemoteClient::addCollector(const QString& channelName, const QJsonObject& config) {
    post(QString("/api/channels/%1/collectors").arg(channelName), config);
}

void RemoteClient::removeCollector(const QString& channelName, const QString& collectorName) {
    del(QString("/api/channels/%1/collectors/%2").arg(channelName).arg(collectorName));
}

// ============================================================================
// 服务器管理 API
// ============================================================================

void RemoteClient::getServers(const QString& channelName) {
    get(QString("/api/channels/%1/servers").arg(channelName));
}

void RemoteClient::addServer(const QString& channelName, const QJsonObject& config) {
    post(QString("/api/channels/%1/servers").arg(channelName), config);
}

// ============================================================================
// 数据访问 API
// ============================================================================

void RemoteClient::getData(const QString& channelName) {
    get(QString("/api/data/%1").arg(channelName));
}

void RemoteClient::getDataPoint(const QString& channelName, const QString& tagName) {
    get(QString("/api/data/%1/%2").arg(channelName).arg(tagName));
}

void RemoteClient::writeDataPoint(const QString& channelName, const QString& tagName, const QVariant& value) {
    QJsonObject body;
    body["value"] = QJsonValue::fromVariant(value);
    post(QString("/api/data/%1/%2").arg(channelName).arg(tagName), body);
}

// ============================================================================
// 配置管理 API
// ============================================================================

void RemoteClient::getConfig() {
    get("/api/config");
}

void RemoteClient::setConfig(const QJsonObject& config) {
    put("/api/config", config);
}

void RemoteClient::saveConfig() {
    post("/api/config/save");
}

void RemoteClient::loadConfig(const QString& filename) {
    post(QString("/api/config/load/%1").arg(filename));
}

// ============================================================================
// 系统信息 API
// ============================================================================

void RemoteClient::getSystemInfo() {
    get("/api/system");
}

void RemoteClient::getStatistics() {
    get("/api/statistics");
}

// ============================================================================
// 报警管理 API
// ============================================================================

void RemoteClient::getAlarms() {
    get("/api/alarms");
}

void RemoteClient::acknowledgeAlarm(const QString& alarmId) {
    post(QString("/api/alarms/%1/ack").arg(alarmId));
}

// ============================================================================
// WebSocket订阅
// ============================================================================

void RemoteClient::subscribeToData() {
    QJsonObject msg;
    msg["topic"] = "data";
    sendWsMessage("subscribe", msg);
}

void RemoteClient::subscribeToMessages() {
    QJsonObject msg;
    msg["topic"] = "messages";
    sendWsMessage("subscribe", msg);
}

void RemoteClient::subscribeToLogs() {
    QJsonObject msg;
    msg["topic"] = "logs";
    sendWsMessage("subscribe", msg);
}

void RemoteClient::subscribeToAlarms() {
    QJsonObject msg;
    msg["topic"] = "alarms";
    sendWsMessage("subscribe", msg);
}

void RemoteClient::subscribeToStatus() {
    QJsonObject msg;
    msg["topic"] = "status";
    sendWsMessage("subscribe", msg);
}

void RemoteClient::subscribeToChannel() {
    QJsonObject msg;
    msg["topic"] = "channel";
    sendWsMessage("subscribe", msg);
}

void RemoteClient::subscribeToMessage() {
    QJsonObject msg;
    msg["topic"] = "message";
    sendWsMessage("subscribe", msg);
}

void RemoteClient::unsubscribe(const QString& topic) {
    QJsonObject msg;
    msg["topic"] = topic;
    sendWsMessage("unsubscribe", msg);
}

void RemoteClient::sendWsMessage(const QString& type, const QJsonObject& data) {
    if (!m_webSocket->isValid()) return;
    
    QJsonObject message;
    message["type"] = type;
    if (!data.isEmpty()) {
        for (auto it = data.begin(); it != data.end(); ++it) {
            message[it.key()] = it.value();
        }
    }
    
    m_webSocket->sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));
}

// ============================================================================
// WebSocket事件处理
// ============================================================================

void RemoteClient::onWsConnected() {
    qDebug() << "RemoteClient: WebSocket connected";
    setState(Connected);
    m_reconnectTimer->stop();
    m_pingTimer->start(30000);  // 每30秒发送心跳
    emit connected();
}

void RemoteClient::onWsDisconnected() {
    qDebug() << "RemoteClient: WebSocket disconnected";
    m_pingTimer->stop();
    
    if (m_state == Connected) {
        setState(Disconnected);
        emit disconnected();
        
        if (m_autoReconnect) {
            qDebug() << "RemoteClient: Will reconnect in" << m_reconnectInterval << "ms";
            m_reconnectTimer->start(m_reconnectInterval);
        }
    }
}

void RemoteClient::onWsTextMessageReceived(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;
    
    QJsonObject msg = doc.object();
    QString type = msg["type"].toString();
    QJsonObject data = msg["data"].toObject();
    
    if (type == "welcome") {
        qDebug() << "RemoteClient: Welcome received, clientId:" << data["clientId"].toString();
    }
    else if (type == "pong") {
        // 心跳响应
    }
    else if (type == "data") {
        QString channel = data["channel"].toString();
        emit realtimeDataReceived(channel, data["points"].toObject());
    }
    else if (type == "message") {
        // 远程Modbus报文
        emit realtimeMessageReceived(data);
    }
    else if (type == "channel") {
        // 通道状态变化（创建/删除/状态变化）
        emit realtimeChannelEventReceived(data);
    }
    else if (type == "messages") {
        emit realtimeMessageReceived(data);
    }
    else if (type == "logs") {
        emit realtimeLogReceived(data);
    }
    else if (type == "alarms") {
        emit realtimeAlarmReceived(data);
    }
    else if (type == "status") {
        emit realtimeStatusReceived(data);
    }
}

void RemoteClient::onWsError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    QString errorString = m_webSocket->errorString();
    qWarning() << "RemoteClient: WebSocket error:" << errorString;
    setError(errorString);
    emit connectionError(errorString);
}

void RemoteClient::onReconnectTimer() {
    if (m_state != Connected) {
        qDebug() << "RemoteClient: Attempting to reconnect...";
        connectToHost(m_host, m_httpPort, m_wsPort);
    }
}

// ============================================================================
// 状态管理
// ============================================================================

void RemoteClient::setState(ConnectionState state) {
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

void RemoteClient::setError(const QString& error) {
    m_lastError = error;
    setState(Error);
}

} // namespace ModbusPlexLink
