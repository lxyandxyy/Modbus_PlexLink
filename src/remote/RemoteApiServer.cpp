#include "RemoteApiServer.h"
#include "core/ChannelManager.h"
#include "core/Channel.h"
#include "core/UniversalDataModel.h"
#include "core/DataPoint.h"
#include "utils/AlarmManager.h"
#include <QHostAddress>
#include <QDateTime>
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QThread>
#include <QUuid>
#include <QUrl>

namespace {
// 辅助函数：ChannelState转字符串
QString channelStateToString(ModbusPlexLink::ChannelState state) {
    switch (state) {
        case ModbusPlexLink::ChannelState::Stopped: return "Stopped";
        case ModbusPlexLink::ChannelState::Starting: return "Starting";
        case ModbusPlexLink::ChannelState::Running: return "Running";
        case ModbusPlexLink::ChannelState::Stopping: return "Stopping";
        case ModbusPlexLink::ChannelState::Error: return "Error";
        default: return "Unknown";
    }
}

// 辅助函数：DataQuality转字符串
QString dataQualityToString(ModbusPlexLink::DataQuality quality) {
    switch (quality) {
        case ModbusPlexLink::DataQuality::Good: return "Good";
        case ModbusPlexLink::DataQuality::Bad: return "Bad";
        case ModbusPlexLink::DataQuality::Uncertain: return "Uncertain";
        case ModbusPlexLink::DataQuality::NotUpdated: return "NotUpdated";
        default: return "Unknown";
    }
}

// 辅助函数：时间戳转ISO字符串
QString timestampToIsoString(qint64 timestamp) {
    return QDateTime::fromMSecsSinceEpoch(timestamp).toString(Qt::ISODate);
}
}

namespace ModbusPlexLink {

// ============================================================================
// HttpResponse 实现
// ============================================================================

void HttpResponse::setJson(const QJsonObject& json) {
    body = QJsonDocument(json).toJson(QJsonDocument::Compact);
    headers["Content-Type"] = "application/json; charset=utf-8";
}

void HttpResponse::setJson(const QJsonArray& json) {
    body = QJsonDocument(json).toJson(QJsonDocument::Compact);
    headers["Content-Type"] = "application/json; charset=utf-8";
}

void HttpResponse::setError(int code, const QString& message) {
    statusCode = code;
    switch (code) {
        case 400: statusText = "Bad Request"; break;
        case 401: statusText = "Unauthorized"; break;
        case 403: statusText = "Forbidden"; break;
        case 404: statusText = "Not Found"; break;
        case 405: statusText = "Method Not Allowed"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Error"; break;
    }
    QJsonObject errorObj;
    errorObj["success"] = false;
    errorObj["error"] = message;
    errorObj["code"] = code;
    setJson(errorObj);
}

void HttpResponse::setSuccess(const QString& message) {
    statusCode = 200;
    statusText = "OK";
    QJsonObject obj;
    obj["success"] = true;
    obj["message"] = message;
    setJson(obj);
}

QByteArray HttpResponse::toBytes() const {
    QByteArray response;
    response.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    response.append("Access-Control-Allow-Origin: *\r\n");
    response.append("Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n");
    response.append("Access-Control-Allow-Headers: Content-Type, Authorization\r\n");
    response.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        response.append(QString("%1: %2\r\n").arg(it.key()).arg(it.value()).toUtf8());
    }
    
    response.append("\r\n");
    response.append(body);
    return response;
}

// ============================================================================
// HttpConnection 实现
// ============================================================================

HttpConnection::HttpConnection(QTcpSocket* socket, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &HttpConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &HttpConnection::onDisconnected);
}

HttpConnection::~HttpConnection() {
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
    }
}

void HttpConnection::onReadyRead() {
    m_buffer.append(m_socket->readAll());
    
    // 检查是否收到完整的HTTP请求
    int headerEnd = m_buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        return;  // 头部还没收完
    }
    
    HttpRequest request;
    if (parseRequest(m_buffer, request)) {
        emit requestReceived(request, this);
    } else {
        HttpResponse response;
        response.setError(400, "Invalid HTTP request");
        sendResponse(response);
    }
    
    m_buffer.clear();
}

void HttpConnection::onDisconnected() {
    emit disconnected();
}

void HttpConnection::sendResponse(const HttpResponse& response) {
    if (m_socket && m_socket->isOpen()) {
        m_socket->write(response.toBytes());
        m_socket->flush();
        m_socket->close();
    }
}

bool HttpConnection::parseRequest(const QByteArray& data, HttpRequest& request) {
    QString dataStr = QString::fromUtf8(data);
    QStringList lines = dataStr.split("\r\n");
    
    if (lines.isEmpty()) return false;
    
    // 解析请求行
    QStringList requestLine = lines[0].split(" ");
    if (requestLine.size() < 3) return false;
    
    request.method = requestLine[0].toUpper();
    QString fullPath = requestLine[1];
    request.version = requestLine[2];
    
    // 解析路径和查询参数
    int queryStart = fullPath.indexOf('?');
    if (queryStart != -1) {
        request.path = QUrl::fromPercentEncoding(fullPath.left(queryStart).toUtf8());
        parseQueryString(fullPath.mid(queryStart + 1), request.queryParams);
    } else {
        request.path = QUrl::fromPercentEncoding(fullPath.toUtf8());
    }
    
    // 解析头部
    int i = 1;
    for (; i < lines.size(); ++i) {
        if (lines[i].isEmpty()) break;
        int colonPos = lines[i].indexOf(':');
        if (colonPos > 0) {
            QString key = lines[i].left(colonPos).trimmed();
            QString value = lines[i].mid(colonPos + 1).trimmed();
            request.headers[key] = value;
        }
    }
    
    // 解析请求体
    int bodyStart = data.indexOf("\r\n\r\n");
    if (bodyStart != -1) {
        request.body = data.mid(bodyStart + 4);
    }
    
    return true;
}

void HttpConnection::parseQueryString(const QString& queryString, QMap<QString, QString>& params) {
    QStringList pairs = queryString.split('&');
    for (const QString& pair : pairs) {
        int eqPos = pair.indexOf('=');
        if (eqPos > 0) {
            QString key = QUrl::fromPercentEncoding(pair.left(eqPos).toUtf8());
            QString value = QUrl::fromPercentEncoding(pair.mid(eqPos + 1).toUtf8());
            params[key] = value;
        }
    }
}

// ============================================================================
// WebSocketClient 实现
// ============================================================================

WebSocketClient::WebSocketClient(QWebSocket* socket, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
    , m_clientId(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8))
{
    connect(m_socket, &QWebSocket::textMessageReceived, 
            this, &WebSocketClient::onTextMessageReceived);
    connect(m_socket, &QWebSocket::disconnected, 
            this, &WebSocketClient::onDisconnected);
}

WebSocketClient::~WebSocketClient() {
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
    }
}

void WebSocketClient::sendMessage(const QJsonObject& message) {
    if (m_socket && m_socket->isValid()) {
        m_socket->sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));
    }
}

void WebSocketClient::sendMessage(const QString& type, const QJsonObject& data) {
    QJsonObject message;
    message["type"] = type;
    message["data"] = data;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    sendMessage(message);
}

void WebSocketClient::subscribe(const QString& topic) {
    if (!m_subscriptions.contains(topic)) {
        m_subscriptions.append(topic);
    }
}

void WebSocketClient::unsubscribe(const QString& topic) {
    m_subscriptions.removeAll(topic);
}

bool WebSocketClient::isSubscribed(const QString& topic) const {
    return m_subscriptions.contains(topic);
}

QStringList WebSocketClient::getSubscriptions() const {
    return m_subscriptions;
}

void WebSocketClient::onTextMessageReceived(const QString& message) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        emit messageReceived(doc.object(), this);
    }
}

void WebSocketClient::onDisconnected() {
    emit disconnected(this);
}

// ============================================================================
// RemoteApiServer 实现
// ============================================================================

RemoteApiServer::RemoteApiServer(ChannelManager* channelManager, 
                                 AlarmManager* alarmManager,
                                 QObject* parent)
    : QObject(parent)
    , m_channelManager(channelManager)
    , m_alarmManager(alarmManager)
    , m_httpServer(new QTcpServer(this))
    , m_wsServer(new QWebSocketServer("ModbusPlexLink", QWebSocketServer::NonSecureMode, this))
    , m_pushTimer(new QTimer(this))
{
    connect(m_httpServer, &QTcpServer::newConnection, 
            this, &RemoteApiServer::onHttpConnection);
    connect(m_wsServer, &QWebSocketServer::newConnection, 
            this, &RemoteApiServer::onWsConnection);
    connect(m_pushTimer, &QTimer::timeout, 
            this, &RemoteApiServer::pushRealtimeData);
    
    // 监听通道管理器的信号，用于连接新通道的状态和报文信号
    if (m_channelManager) {
        connect(m_channelManager, &ChannelManager::channelCreated,
                this, &RemoteApiServer::onChannelCreated);
        connect(m_channelManager, &ChannelManager::channelDeleted,
                this, &RemoteApiServer::onChannelDeleted);
        
        // 连接现有通道的信号
        for (Channel* channel : m_channelManager->getAllChannels()) {
            connectChannelSignals(channel);
        }
    }
}

RemoteApiServer::~RemoteApiServer() {
    stop();
}

bool RemoteApiServer::start(quint16 httpPort, quint16 wsPort) {
    m_httpPort = httpPort;
    m_wsPort = wsPort;
    
    // 启动HTTP服务器
    if (!m_httpServer->listen(QHostAddress::Any, httpPort)) {
        emit serverError(QString("HTTP服务器启动失败: %1").arg(m_httpServer->errorString()));
        return false;
    }
    
    // 启动WebSocket服务器
    if (!m_wsServer->listen(QHostAddress::Any, wsPort)) {
        m_httpServer->close();
        emit serverError(QString("WebSocket服务器启动失败: %1").arg(m_wsServer->errorString()));
        return false;
    }
    
    // 启动数据推送定时器
    m_pushTimer->start(m_pushInterval);
    
    qInfo() << "RemoteApiServer started:";
    qInfo() << "  HTTP API:" << getHttpAddress();
    qInfo() << "  WebSocket:" << getWebSocketAddress();
    
    return true;
}

void RemoteApiServer::stop() {
    m_pushTimer->stop();
    
    // 关闭所有WebSocket连接
    for (WebSocketClient* client : m_wsClients) {
        client->deleteLater();
    }
    m_wsClients.clear();
    
    // 关闭所有HTTP连接
    for (HttpConnection* conn : m_httpConnections) {
        conn->deleteLater();
    }
    m_httpConnections.clear();
    
    m_wsServer->close();
    m_httpServer->close();
    
    qInfo() << "RemoteApiServer stopped";
}

bool RemoteApiServer::isRunning() const {
    return m_httpServer->isListening() && m_wsServer->isListening();
}

QString RemoteApiServer::getHttpAddress() const {
    QString ip = "0.0.0.0";
    // 尝试获取本机IP
    for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            ip = addr.toString();
            break;
        }
    }
    return QString("http://%1:%2").arg(ip).arg(m_httpPort);
}

QString RemoteApiServer::getWebSocketAddress() const {
    QString ip = "0.0.0.0";
    for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            ip = addr.toString();
            break;
        }
    }
    return QString("ws://%1:%2").arg(ip).arg(m_wsPort);
}

void RemoteApiServer::setAuthentication(bool enabled, const QString& username, const QString& password) {
    m_authEnabled = enabled;
    m_authUsername = username;
    m_authPassword = password;
}

// ============================================================================
// HTTP请求处理
// ============================================================================

void RemoteApiServer::onHttpConnection() {
    while (m_httpServer->hasPendingConnections()) {
        QTcpSocket* socket = m_httpServer->nextPendingConnection();
        HttpConnection* conn = new HttpConnection(socket, this);
        m_httpConnections.append(conn);
        
        connect(conn, &HttpConnection::requestReceived, 
                this, &RemoteApiServer::handleHttpRequest);
        connect(conn, &HttpConnection::disconnected, this, [this, conn]() {
            m_httpConnections.removeAll(conn);
            conn->deleteLater();
        });
    }
}

void RemoteApiServer::handleHttpRequest(const HttpRequest& request, HttpConnection* connection) {
    emit requestReceived(request.method, request.path);
    
    HttpResponse response;
    
    // 处理OPTIONS请求（CORS预检）
    if (request.method == "OPTIONS") {
        response.statusCode = 204;
        response.statusText = "No Content";
        connection->sendResponse(response);
        return;
    }
    
    // 认证检查
    if (m_authEnabled && !checkAuth(request)) {
        response.setError(401, "Unauthorized");
        response.headers["WWW-Authenticate"] = "Basic realm=\"ModbusPlexLink\"";
        connection->sendResponse(response);
        return;
    }
    
    QStringList params;
    QString method = request.method;
    QString path = request.path;
    
    // 解析请求体
    QJsonObject bodyJson;
    if (!request.body.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(request.body);
        if (doc.isObject()) {
            bodyJson = doc.object();
        }
    }
    
    // ========== 路由匹配 ==========
    
    // 系统信息
    if (path == "/api/system" && method == "GET") {
        response = handleGetSystemInfo();
    }
    // 统计信息
    else if (path == "/api/statistics" && method == "GET") {
        response = handleGetStatistics();
    }
    // 配置管理
    else if (path == "/api/config" && method == "GET") {
        response = handleGetConfig();
    }
    else if (path == "/api/config" && method == "PUT") {
        response = handleSetConfig(bodyJson);
    }
    else if (path == "/api/config/save" && method == "POST") {
        response = handleSaveConfig();
    }
    else if (matchRoute("/api/config/load/:filename", path, params) && method == "POST") {
        response = handleLoadConfig(params[0]);
    }
    // 通道管理
    else if (path == "/api/channels" && method == "GET") {
        response = handleGetChannels();
    }
    else if (path == "/api/channels" && method == "POST") {
        response = handleCreateChannel(bodyJson);
    }
    else if (matchRoute("/api/channels/:name", path, params) && method == "GET") {
        response = handleGetChannel(params[0]);
    }
    else if (matchRoute("/api/channels/:name", path, params) && method == "PUT") {
        response = handleUpdateChannel(params[0], bodyJson);
    }
    else if (matchRoute("/api/channels/:name", path, params) && method == "DELETE") {
        response = handleDeleteChannel(params[0]);
    }
    else if (matchRoute("/api/channels/:name/start", path, params) && method == "POST") {
        response = handleStartChannel(params[0]);
    }
    else if (matchRoute("/api/channels/:name/stop", path, params) && method == "POST") {
        response = handleStopChannel(params[0]);
    }
    // 采集器管理
    else if (matchRoute("/api/channels/:name/collectors", path, params) && method == "GET") {
        response = handleGetCollectors(params[0]);
    }
    else if (matchRoute("/api/channels/:name/collectors", path, params) && method == "POST") {
        response = handleAddCollector(params[0], bodyJson);
    }
    // 服务器管理
    else if (matchRoute("/api/channels/:name/servers", path, params) && method == "GET") {
        response = handleGetServers(params[0]);
    }
    else if (matchRoute("/api/channels/:name/servers", path, params) && method == "POST") {
        response = handleAddServer(params[0], bodyJson);
    }
    // 数据访问
    else if (matchRoute("/api/data/:channel", path, params) && method == "GET") {
        response = handleGetData(params[0]);
    }
    else if (matchRoute("/api/data/:channel/:tag", path, params) && method == "GET") {
        response = handleGetDataPoint(params[0], params[1]);
    }
    else if (matchRoute("/api/data/:channel/:tag", path, params) && method == "POST") {
        response = handleWriteDataPoint(params[0], params[1], bodyJson);
    }
    // 报警管理
    else if (path == "/api/alarms" && method == "GET") {
        response = handleGetAlarms();
    }
    else if (matchRoute("/api/alarms/:id/ack", path, params) && method == "POST") {
        response = handleAcknowledgeAlarm(params[0]);
    }
    // 404
    else {
        response.setError(404, QString("Endpoint not found: %1 %2").arg(method).arg(path));
    }
    
    connection->sendResponse(response);
}

// ============================================================================
// API处理函数实现
// ============================================================================

HttpResponse RemoteApiServer::handleGetChannels() {
    HttpResponse response;
    QJsonArray channels;
    
    for (const QString& name : m_channelManager->getChannelNames()) {
        Channel* channel = m_channelManager->getChannel(name);
        if (channel) {
            QJsonObject ch;
            ch["name"] = name;
            ch["running"] = channel->isRunning();
            ch["state"] = static_cast<int>(channel->getState());
            ch["collectorCount"] = channel->getCollectors().size();
            ch["serverCount"] = channel->getServers().size();
            ch["dataPointCount"] = channel->getDataModel() ? channel->getDataModel()->size() : 0;
            channels.append(ch);
        }
    }
    
    QJsonObject result;
    result["success"] = true;
    result["channels"] = channels;
    result["count"] = channels.size();
    response.setJson(result);
    return response;
}

HttpResponse RemoteApiServer::handleGetChannel(const QString& name) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(name);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(name));
        return response;
    }
    
    // 获取通道的完整配置
    ChannelConfig config = channel->getConfig();
    
    QJsonObject ch;
    ch["name"] = name;
    ch["enabled"] = config.enabled;
    ch["description"] = config.description;
    ch["running"] = channel->isRunning();
    ch["state"] = static_cast<int>(channel->getState());
    ch["stateString"] = channelStateToString(channel->getState());
    
    // 采集器配置（完整配置，用于编辑）
    QJsonArray collectorsConfig;
    for (const QJsonObject& collConfig : config.collectors) {
        collectorsConfig.append(collConfig);
    }
    ch["collectors"] = collectorsConfig;
    
    // 服务器配置（完整配置，用于编辑）
    QJsonArray serversConfig;
    for (const QJsonObject& srvConfig : config.servers) {
        serversConfig.append(srvConfig);
    }
    ch["servers"] = serversConfig;
    
    // 运行时状态信息
    QJsonArray collectorsStatus;
    for (ICollector* collector : channel->getCollectors()) {
        QJsonObject coll;
        coll["name"] = collector->getName();
        coll["connected"] = collector->isConnected();
        coll["running"] = collector->isRunning();
        coll["statistics"] = collector->getStatistics();
        collectorsStatus.append(coll);
    }
    ch["collectorsStatus"] = collectorsStatus;
    
    QJsonArray serversStatus;
    for (IServer* server : channel->getServers()) {
        QJsonObject srv;
        srv["name"] = server->getName();
        srv["running"] = server->isRunning();
        srv["statistics"] = server->getStatistics();
        serversStatus.append(srv);
    }
    ch["serversStatus"] = serversStatus;
    
    // 数据点统计
    if (channel->getDataModel()) {
        ch["dataPointCount"] = channel->getDataModel()->size();
    }
    
    QJsonObject result;
    result["success"] = true;
    result["channel"] = ch;
    response.setJson(result);
    return response;
}

HttpResponse RemoteApiServer::handleCreateChannel(const QJsonObject& body) {
    HttpResponse response;
    QString name = body["name"].toString();
    
    if (name.isEmpty()) {
        response.setError(400, "Channel name is required");
        return response;
    }
    
    if (m_channelManager->getChannel(name)) {
        response.setError(400, QString("Channel already exists: %1").arg(name));
        return response;
    }
    
    // 创建通道
    Channel* channel = m_channelManager->createChannel(name);
    if (!channel) {
        response.setError(500, "Failed to create channel");
        return response;
    }
    
    // 如果提供了完整配置，应用配置
    if (body.contains("collectors") || body.contains("servers")) {
        ChannelConfig config;
        config.name = name;
        config.enabled = body["enabled"].toBool(true);
        config.description = body["description"].toString();
        
        // 解析采集器配置
        QJsonArray collectorsArray = body["collectors"].toArray();
        for (const QJsonValue& val : collectorsArray) {
            config.collectors.append(val.toObject());
        }
        
        // 解析服务器配置
        QJsonArray serversArray = body["servers"].toArray();
        for (const QJsonValue& val : serversArray) {
            config.servers.append(val.toObject());
        }
        
        // 应用配置
        if (!channel->configure(config)) {
            qWarning() << "Failed to configure new channel:" << name;
        }
    }
    
    response.setSuccess(QString("Channel created: %1").arg(name));
    return response;
}

HttpResponse RemoteApiServer::handleUpdateChannel(const QString& name, const QJsonObject& body) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(name);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(name));
        return response;
    }
    
    // 记录通道之前的运行状态
    bool wasRunning = channel->isRunning();
    
    // 如果通道正在运行，先停止
    if (wasRunning) {
        channel->stop();
        // 等待通道停止（最多3秒）
        int waitCount = 0;
        while (channel->getState() != ChannelState::Stopped && waitCount < 30) {
            QThread::msleep(100);
            waitCount++;
        }
    }
    
    // 构建新配置
    ChannelConfig config;
    config.name = body.contains("name") ? body["name"].toString() : name;
    config.enabled = body["enabled"].toBool(true);
    config.description = body["description"].toString();
    
    // 解析采集器配置
    QJsonArray collectorsArray = body["collectors"].toArray();
    for (const QJsonValue& val : collectorsArray) {
        config.collectors.append(val.toObject());
    }
    
    // 解析服务器配置
    QJsonArray serversArray = body["servers"].toArray();
    for (const QJsonValue& val : serversArray) {
        config.servers.append(val.toObject());
    }
    
    // 应用配置
    if (!channel->configure(config)) {
        response.setError(500, QString("Failed to configure channel: %1").arg(name));
        // 尝试恢复运行状态
        if (wasRunning) {
            channel->start();
        }
        return response;
    }
    
    // 如果原来在运行，重新启动
    if (wasRunning) {
        if (!channel->start()) {
            response.setError(500, QString("Channel configured but failed to restart: %1").arg(name));
            return response;
        }
    }
    
    response.setSuccess(QString("Channel updated: %1").arg(name));
    return response;
}

HttpResponse RemoteApiServer::handleDeleteChannel(const QString& name) {
    HttpResponse response;
    
    if (!m_channelManager->getChannel(name)) {
        response.setError(404, QString("Channel not found: %1").arg(name));
        return response;
    }
    
    if (m_channelManager->deleteChannel(name)) {
        response.setSuccess(QString("Channel deleted: %1").arg(name));
    } else {
        response.setError(500, "Failed to delete channel");
    }
    return response;
}

HttpResponse RemoteApiServer::handleStartChannel(const QString& name) {
    HttpResponse response;
    
    if (!m_channelManager->getChannel(name)) {
        response.setError(404, QString("Channel not found: %1").arg(name));
        return response;
    }
    
    if (m_channelManager->startChannel(name)) {
        response.setSuccess(QString("Channel started: %1").arg(name));
    } else {
        response.setError(500, "Failed to start channel");
    }
    return response;
}

HttpResponse RemoteApiServer::handleStopChannel(const QString& name) {
    HttpResponse response;
    
    if (!m_channelManager->getChannel(name)) {
        response.setError(404, QString("Channel not found: %1").arg(name));
        return response;
    }
    
    if (m_channelManager->stopChannel(name)) {
        response.setSuccess(QString("Channel stopped: %1").arg(name));
    } else {
        response.setError(500, "Failed to stop channel");
    }
    return response;
}

HttpResponse RemoteApiServer::handleGetCollectors(const QString& channelName) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(channelName);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(channelName));
        return response;
    }
    
    QJsonArray collectors;
    for (ICollector* collector : channel->getCollectors()) {
        QJsonObject coll;
        coll["name"] = collector->getName();
        coll["connected"] = collector->isConnected();
        coll["running"] = collector->isRunning();
        coll["statistics"] = collector->getStatistics();
        collectors.append(coll);
    }
    
    QJsonObject result;
    result["success"] = true;
    result["collectors"] = collectors;
    response.setJson(result);
    return response;
}

HttpResponse RemoteApiServer::handleAddCollector(const QString& channelName, const QJsonObject& body) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(channelName);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(channelName));
        return response;
    }
    
    if (channel->addCollector(body)) {
        response.setSuccess("Collector added");
    } else {
        response.setError(500, "Failed to add collector");
    }
    return response;
}

HttpResponse RemoteApiServer::handleUpdateCollector(const QString& channelName, 
                                                    const QString& collectorName, 
                                                    const QJsonObject& body) {
    HttpResponse response;
    // TODO: 实现采集器更新
    response.setSuccess("Collector updated");
    return response;
}

HttpResponse RemoteApiServer::handleDeleteCollector(const QString& channelName, 
                                                    const QString& collectorName) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(channelName);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(channelName));
        return response;
    }
    
    if (channel->removeCollector(collectorName)) {
        response.setSuccess("Collector removed");
    } else {
        response.setError(404, "Collector not found");
    }
    return response;
}

HttpResponse RemoteApiServer::handleGetServers(const QString& channelName) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(channelName);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(channelName));
        return response;
    }
    
    QJsonArray servers;
    for (IServer* server : channel->getServers()) {
        QJsonObject srv;
        srv["name"] = server->getName();
        srv["running"] = server->isRunning();
        srv["statistics"] = server->getStatistics();
        servers.append(srv);
    }
    
    QJsonObject result;
    result["success"] = true;
    result["servers"] = servers;
    response.setJson(result);
    return response;
}

HttpResponse RemoteApiServer::handleAddServer(const QString& channelName, const QJsonObject& body) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(channelName);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(channelName));
        return response;
    }
    
    if (channel->addServer(body)) {
        response.setSuccess("Server added");
    } else {
        response.setError(500, "Failed to add server");
    }
    return response;
}

HttpResponse RemoteApiServer::handleGetData(const QString& channelName) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(channelName);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(channelName));
        return response;
    }
    
    UniversalDataModel* udm = channel->getDataModel();
    if (!udm) {
        response.setError(500, "Data model not available");
        return response;
    }
    
    QJsonObject dataPoints;
    QStringList tags = udm->getAllTags();
    
    for (const QString& tag : tags) {
        DataPoint dp = udm->readPoint(tag);
        QJsonObject point;
        point["value"] = QJsonValue::fromVariant(dp.value);
        point["quality"] = static_cast<int>(dp.quality);
        point["timestamp"] = timestampToIsoString(dp.timestamp);
        dataPoints[tag] = point;
    }
    
    QJsonObject result;
    result["success"] = true;
    result["channel"] = channelName;
    result["data"] = dataPoints;
    result["count"] = tags.size();
    response.setJson(result);
    return response;
}

HttpResponse RemoteApiServer::handleGetDataPoint(const QString& channelName, const QString& tagName) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(channelName);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(channelName));
        return response;
    }
    
    UniversalDataModel* udm = channel->getDataModel();
    if (!udm) {
        response.setError(500, "Data model not available");
        return response;
    }
    
    if (!udm->hasTag(tagName)) {
        response.setError(404, QString("Tag not found: %1").arg(tagName));
        return response;
    }
    
    DataPoint dp = udm->readPoint(tagName);
    QJsonObject result;
    result["success"] = true;
    result["tag"] = tagName;
    result["value"] = QJsonValue::fromVariant(dp.value);
    result["quality"] = static_cast<int>(dp.quality);
    result["qualityString"] = dataQualityToString(dp.quality);
    result["timestamp"] = timestampToIsoString(dp.timestamp);
    response.setJson(result);
    return response;
}

HttpResponse RemoteApiServer::handleWriteDataPoint(const QString& channelName, 
                                                   const QString& tagName, 
                                                   const QJsonObject& body) {
    HttpResponse response;
    Channel* channel = m_channelManager->getChannel(channelName);
    
    if (!channel) {
        response.setError(404, QString("Channel not found: %1").arg(channelName));
        return response;
    }
    
    if (!body.contains("value")) {
        response.setError(400, "Value is required");
        return response;
    }
    
    QVariant value = body["value"].toVariant();
    
    // 写入数据点
    DataPoint dp(value, DataQuality::Good);
    channel->getDataModel()->updatePoint(tagName, dp);
    
    response.setSuccess(QString("Value written to %1").arg(tagName));
    return response;
}

HttpResponse RemoteApiServer::handleGetConfig() {
    HttpResponse response;
    QJsonObject config = m_channelManager->getConfig();
    
    QJsonObject result;
    result["success"] = true;
    result["config"] = config;
    response.setJson(result);
    return response;
}

HttpResponse RemoteApiServer::handleSetConfig(const QJsonObject& body) {
    HttpResponse response;
    
    if (m_channelManager->loadConfig(body)) {
        response.setSuccess("Configuration applied");
    } else {
        response.setError(500, "Failed to apply configuration");
    }
    return response;
}

HttpResponse RemoteApiServer::handleSaveConfig() {
    HttpResponse response;
    
    if (m_channelManager->saveConfig("config.json")) {
        response.setSuccess("Configuration saved");
    } else {
        response.setError(500, "Failed to save configuration");
    }
    return response;
}

HttpResponse RemoteApiServer::handleLoadConfig(const QString& filename) {
    HttpResponse response;
    
    if (m_channelManager->loadConfig(filename)) {
        response.setSuccess(QString("Configuration loaded from %1").arg(filename));
    } else {
        response.setError(500, QString("Failed to load configuration from %1").arg(filename));
    }
    return response;
}

HttpResponse RemoteApiServer::handleGetStatistics() {
    HttpResponse response;
    QJsonObject stats = m_channelManager->getGlobalStatistics();
    
    QJsonObject result;
    result["success"] = true;
    result["statistics"] = stats;
    response.setJson(result);
    return response;
}

HttpResponse RemoteApiServer::handleGetAlarms() {
    HttpResponse response;
    
    if (!m_alarmManager) {
        response.setError(500, "Alarm manager not available");
        return response;
    }
    
    QJsonArray alarms;
    for (const AlarmEvent& event : m_alarmManager->getActiveAlarms()) {
        QJsonObject alarm;
        alarm["id"] = event.id;
        alarm["ruleId"] = event.ruleId;
        alarm["ruleName"] = event.ruleName;
        alarm["channelName"] = event.channelName;
        alarm["tagName"] = event.tagName;
        alarm["value"] = QJsonValue::fromVariant(event.value);
        alarm["message"] = event.message;
        alarm["priority"] = static_cast<int>(event.priority);
        alarm["state"] = static_cast<int>(event.state);
        alarm["activeTime"] = event.activeTime.toString(Qt::ISODate);
        alarm["acknowledged"] = (event.state == AlarmState::Acknowledged);
        alarms.append(alarm);
    }
    
    QJsonObject result;
    result["success"] = true;
    result["alarms"] = alarms;
    result["count"] = alarms.size();
    response.setJson(result);
    return response;
}

HttpResponse RemoteApiServer::handleAcknowledgeAlarm(const QString& alarmId) {
    HttpResponse response;
    
    if (!m_alarmManager) {
        response.setError(500, "Alarm manager not available");
        return response;
    }
    
    if (m_alarmManager->acknowledgeAlarm(alarmId)) {
        response.setSuccess(QString("Alarm acknowledged: %1").arg(alarmId));
    } else {
        response.setError(404, QString("Alarm not found: %1").arg(alarmId));
    }
    return response;
}

HttpResponse RemoteApiServer::handleGetSystemInfo() {
    HttpResponse response;
    
    QJsonObject info;
    info["name"] = QCoreApplication::applicationName();
    info["version"] = QCoreApplication::applicationVersion();
    info["qtVersion"] = qVersion();
    info["platform"] = QSysInfo::prettyProductName();
    info["hostname"] = QSysInfo::machineHostName();
    info["uptime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    info["channelCount"] = m_channelManager->getChannelCount();
    info["runningChannels"] = m_channelManager->getRunningChannelCount();
    info["wsClients"] = m_wsClients.size();
    
    QJsonObject result;
    result["success"] = true;
    result["system"] = info;
    response.setJson(result);
    return response;
}

// ============================================================================
// WebSocket处理
// ============================================================================

void RemoteApiServer::onWsConnection() {
    while (m_wsServer->hasPendingConnections()) {
        QWebSocket* socket = m_wsServer->nextPendingConnection();
        WebSocketClient* client = new WebSocketClient(socket, this);
        m_wsClients.append(client);
        
        connect(client, &WebSocketClient::messageReceived, 
                this, &RemoteApiServer::handleWsMessage);
        connect(client, &WebSocketClient::disconnected, 
                this, &RemoteApiServer::onWsClientDisconnected);
        
        emit clientConnected(client->getClientId());
        qInfo() << "WebSocket client connected:" << client->getClientId();
        
        // 发送欢迎消息
        QJsonObject welcome;
        welcome["clientId"] = client->getClientId();
        welcome["message"] = "Connected to ModbusPlexLink Remote API";
        welcome["availableTopics"] = QJsonArray({"data", "messages", "logs", "alarms", "status"});
        client->sendMessage("welcome", welcome);
    }
}

void RemoteApiServer::handleWsMessage(const QJsonObject& message, WebSocketClient* client) {
    QString type = message["type"].toString();
    
    if (type == "subscribe") {
        QString topic = message["topic"].toString();
        client->subscribe(topic);
        
        QJsonObject response;
        response["subscribed"] = topic;
        client->sendMessage("subscribed", response);
        qDebug() << "Client" << client->getClientId() << "subscribed to" << topic;
    }
    else if (type == "unsubscribe") {
        QString topic = message["topic"].toString();
        client->unsubscribe(topic);
        
        QJsonObject response;
        response["unsubscribed"] = topic;
        client->sendMessage("unsubscribed", response);
    }
    else if (type == "ping") {
        QJsonObject pong;
        pong["time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        client->sendMessage("pong", pong);
    }
    else if (type == "getData") {
        QString channelName = message["channel"].toString();
        HttpResponse httpResp = handleGetData(channelName);
        QJsonDocument doc = QJsonDocument::fromJson(httpResp.body);
        client->sendMessage("data", doc.object());
    }
}

void RemoteApiServer::onWsClientDisconnected(WebSocketClient* client) {
    emit clientDisconnected(client->getClientId());
    qInfo() << "WebSocket client disconnected:" << client->getClientId();
    m_wsClients.removeAll(client);
    client->deleteLater();
}

void RemoteApiServer::pushRealtimeData() {
    if (m_wsClients.isEmpty()) return;
    
    // 推送数据更新
    for (const QString& channelName : m_channelManager->getChannelNames()) {
        Channel* channel = m_channelManager->getChannel(channelName);
        if (!channel || !channel->isRunning()) continue;
        
        UniversalDataModel* udm = channel->getDataModel();
        if (!udm) continue;
        
        QJsonObject dataUpdate;
        dataUpdate["channel"] = channelName;
        
        QJsonObject points;
        for (const QString& tag : udm->getAllTags()) {
            DataPoint dp = udm->readPoint(tag);
            QJsonObject point;
            point["value"] = QJsonValue::fromVariant(dp.value);
            point["quality"] = static_cast<int>(dp.quality);
            points[tag] = point;
        }
        dataUpdate["points"] = points;
        
        broadcastToSubscribers("data", dataUpdate);
    }
    
    // 推送状态更新
    QJsonObject statusUpdate;
    statusUpdate["runningChannels"] = m_channelManager->getRunningChannelCount();
    statusUpdate["totalChannels"] = m_channelManager->getChannelCount();
    broadcastToSubscribers("status", statusUpdate);
}

void RemoteApiServer::broadcastToSubscribers(const QString& topic, const QJsonObject& data) {
    for (WebSocketClient* client : m_wsClients) {
        if (client->isSubscribed(topic)) {
            client->sendMessage(topic, data);
        }
    }
}

// ============================================================================
// 辅助方法
// ============================================================================

bool RemoteApiServer::matchRoute(const QString& pattern, const QString& path, QStringList& params) {
    params.clear();
    
    QStringList patternParts = pattern.split('/', Qt::SkipEmptyParts);
    QStringList pathParts = path.split('/', Qt::SkipEmptyParts);
    
    if (patternParts.size() != pathParts.size()) {
        return false;
    }
    
    for (int i = 0; i < patternParts.size(); ++i) {
        if (patternParts[i].startsWith(':')) {
            params.append(pathParts[i]);
        } else if (patternParts[i] != pathParts[i]) {
            return false;
        }
    }
    
    return true;
}

bool RemoteApiServer::checkAuth(const HttpRequest& request) {
    QString authHeader = request.headers.value("Authorization");
    
    if (authHeader.isEmpty()) {
        return false;
    }
    
    if (authHeader.startsWith("Basic ")) {
        QString encoded = authHeader.mid(6);
        QByteArray decoded = QByteArray::fromBase64(encoded.toUtf8());
        QString credentials = QString::fromUtf8(decoded);
        
        int colonPos = credentials.indexOf(':');
        if (colonPos > 0) {
            QString username = credentials.left(colonPos);
            QString password = credentials.mid(colonPos + 1);
            return username == m_authUsername && password == m_authPassword;
        }
    }
    
    return false;
}

// ============================================================================
// 通道信号连接和推送
// ============================================================================

void RemoteApiServer::connectChannelSignals(Channel* channel) {
    if (!channel) return;
    
    // 先断开可能存在的旧连接
    disconnect(channel, &Channel::stateChanged, this, nullptr);
    disconnect(channel, &Channel::modbusMessage, this, nullptr);
    
    // 连接通道状态变化信号
    connect(channel, &Channel::stateChanged, this, [this, channel](ChannelState state) {
        onChannelStateChanged(channel->getName(), state);
    });
    
    // 连接通道Modbus报文信号
    connect(channel, &Channel::modbusMessage, this, 
            [this, channel](const QString& source, const QString& direction,
                          const QString& device, const QString& function,
                          const QString& address, const QString& data, bool success) {
        onChannelModbusMessage(channel->getName(), source, direction, device, 
                              function, address, data, success);
    });
}

void RemoteApiServer::onChannelCreated(const QString& channelName) {
    Channel* channel = m_channelManager->getChannel(channelName);
    if (channel) {
        connectChannelSignals(channel);
        
        // 推送通道创建通知
        QJsonObject message;
        message["channel"] = channelName;
        message["action"] = "created";
        message["state"] = static_cast<int>(channel->getState());
        message["stateString"] = channelStateToString(channel->getState());
        broadcastToSubscribers("channel", message);
    }
}

void RemoteApiServer::onChannelDeleted(const QString& channelName) {
    // 推送通道删除通知
    QJsonObject message;
    message["channel"] = channelName;
    message["action"] = "deleted";
    broadcastToSubscribers("channel", message);
}

void RemoteApiServer::onChannelStateChanged(const QString& channelName, ChannelState state) {
    // 推送通道状态变化
    QJsonObject message;
    message["channel"] = channelName;
    message["action"] = "stateChanged";
    message["state"] = static_cast<int>(state);
    message["stateString"] = channelStateToString(state);
    message["running"] = (state == ChannelState::Running);
    broadcastToSubscribers("channel", message);
}

void RemoteApiServer::onChannelModbusMessage(const QString& channelName, const QString& source,
                                             const QString& direction, const QString& device,
                                             const QString& function, const QString& address,
                                             const QString& data, bool success) {
    // 推送Modbus报文
    QJsonObject message;
    message["channel"] = channelName;
    message["source"] = source;
    message["direction"] = direction;
    message["device"] = device;
    message["function"] = function;
    message["address"] = address;
    message["data"] = data;
    message["success"] = success;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    broadcastToSubscribers("message", message);
}

} // namespace ModbusPlexLink
