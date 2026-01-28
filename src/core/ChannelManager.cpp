#include "ChannelManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QTimer>
#include <QDebug>

namespace ModbusPlexLink {

ChannelManager::ChannelManager(QObject *parent)
    : QObject(parent)
{
}

ChannelManager::~ChannelManager() {
    stopAll();
    
    // 清理所有通道
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        delete it.value();
    }
    m_channels.clear();
}

bool ChannelManager::loadConfig(const QString& configFile) {
    QFile file(configFile);
    if (!file.open(QIODevice::ReadOnly)) {
        emit globalError(QString("Failed to open config file: %1").arg(configFile));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        emit globalError(QString("Failed to parse config file: %1").arg(parseError.errorString()));
        return false;
    }
    
    if (!doc.isObject()) {
        emit globalError("Invalid config file format: root must be an object");
        return false;
    }
    
    m_configFile = configFile;
    return loadConfig(doc.object());
}

bool ChannelManager::loadConfig(const QJsonObject& config) {
    m_config = config;
    
    // 停止并删除所有现有通道
    stopAll();
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        delete it.value();
    }
    m_channels.clear();
    
    // 解析通道配置
    QJsonArray channelsArray = config["channels"].toArray();
    
    for (const QJsonValue& value : channelsArray) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject channelConfig = value.toObject();
        QString channelName = channelConfig["name"].toString();
        
        if (channelName.isEmpty()) {
            qWarning() << "Channel config missing name, skipping";
            continue;
        }
        
        // 创建通道
        Channel* channel = createChannel(channelName);
        if (channel) {
            // 配置通道
            if (!channel->configure(channelConfig)) {
                qWarning() << "Failed to configure channel" << channelName;
                delete channel;
                m_channels.remove(channelName);
            }
            // 注意：加载配置时不自动启动通道，避免阻塞GUI
            // 用户可以通过界面手动启动需要的通道
        }
    }
    
    emit configLoaded();
    return true;
}

bool ChannelManager::saveConfig(const QString& configFile) const {
    QFile file(configFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config file for writing:" << configFile;
        return false;
    }
    
    QJsonDocument doc(getConfig());
    file.write(doc.toJson());
    file.close();
    
    const_cast<ChannelManager*>(this)->m_configFile = configFile;
    const_cast<ChannelManager*>(this)->emit configSaved();
    
    return true;
}

QJsonObject ChannelManager::getConfig() const {
    QJsonObject config;
    
    // 更新通道配置
    QJsonArray channelsArray;
    for (auto it = m_channels.constBegin(); it != m_channels.constEnd(); ++it) {
        Channel* channel = it.value();
        
        // 获取通道的完整配置
        ChannelConfig channelCfg = channel->getConfig();
        
        QJsonObject channelConfig;
        channelConfig["name"] = channelCfg.name;
        channelConfig["enabled"] = channelCfg.enabled;
        channelConfig["description"] = channelCfg.description;
        
        // 保存采集器配置
        QJsonArray collectorsArray;
        for (const QJsonObject& collector : channelCfg.collectors) {
            collectorsArray.append(collector);
        }
        channelConfig["collectors"] = collectorsArray;
        
        // 保存服务器配置
        QJsonArray serversArray;
        for (const QJsonObject& server : channelCfg.servers) {
            serversArray.append(server);
        }
        channelConfig["servers"] = serversArray;
        
        // 保存虚拟化配置（兼容旧格式）
        if (!channelCfg.virtualization.isEmpty()) {
            channelConfig["virtualization"] = channelCfg.virtualization;
        }
        
        channelsArray.append(channelConfig);
    }
    
    config["channels"] = channelsArray;
    config["version"] = "1.0";
    config["description"] = "Modbus PlexLink Configuration";
    
    return config;
}

Channel* ChannelManager::createChannel(const QString& name) {
    if (name.isEmpty()) {
        emit globalError("Cannot create channel with empty name");
        return nullptr;
    }
    
    if (m_channels.contains(name)) {
        emit globalError(QString("Channel %1 already exists").arg(name));
        return nullptr;
    }
    
    Channel* channel = new Channel(name, this);
    
    // 连接信号
    connect(channel, &Channel::stateChanged,
            this, &ChannelManager::onChannelStateChanged);
    connect(channel, &Channel::errorOccurred,
            this, &ChannelManager::onChannelError);
    
    m_channels[name] = channel;
    emit channelCreated(name);
    
    return channel;
}

Channel* ChannelManager::createChannel(const ChannelConfig& config) {
    Channel* channel = createChannel(config.name);
    if (channel) {
        if (!channel->configure(config)) {
            deleteChannel(config.name);
            return nullptr;
        }
    }
    return channel;
}

Channel* ChannelManager::getChannel(const QString& name) {
    return m_channels.value(name, nullptr);
}

const Channel* ChannelManager::getChannel(const QString& name) const {
    return m_channels.value(name, nullptr);
}

QList<Channel*> ChannelManager::getAllChannels() const {
    return m_channels.values();
}

QStringList ChannelManager::getChannelNames() const {
    return m_channels.keys();
}

bool ChannelManager::deleteChannel(const QString& name) {
    auto it = m_channels.find(name);
    if (it == m_channels.end()) {
        return false;
    }
    
    Channel* channel = it.value();
    channel->stop();
    m_channels.erase(it);
    delete channel;
    
    emit channelDeleted(name);
    return true;
}

bool ChannelManager::startChannel(const QString& name) {
    Channel* channel = getChannel(name);
    if (!channel) {
        emit globalError(QString("通道 %1 不存在").arg(name));
        return false;
    }
    
    // 使用QTimer延迟启动，避免阻塞UI（简单且安全的方式）
    QTimer::singleShot(0, this, [this, name]() {
        Channel* channel = getChannel(name);
        if (!channel) {
            return;
        }
        
        qInfo() << "正在启动通道:" << name;
        bool success = channel->start();
        
        if (!success) {
            emit globalError(QString("通道 %1 启动失败，请检查网络连接和配置").arg(name));
        } else {
            qInfo() << "通道" << name << "启动成功";
        }
    });
    
    return true;  // 返回true表示已发起启动请求
}

bool ChannelManager::stopChannel(const QString& name) {
    Channel* channel = getChannel(name);
    if (!channel) {
        emit globalError(QString("Channel %1 not found").arg(name));
        return false;
    }
    
    channel->stop();
    return true;
}

bool ChannelManager::restartChannel(const QString& name) {
    Channel* channel = getChannel(name);
    if (!channel) {
        emit globalError(QString("Channel %1 not found").arg(name));
        return false;
    }
    
    return channel->restart();
}

void ChannelManager::startAll() {
    qInfo() << "正在启动所有通道...";
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        startChannel(it.key());  // 使用异步启动
    }
}

void ChannelManager::stopAll() {
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        it.value()->stop();
    }
}

int ChannelManager::getRunningChannelCount() const {
    int count = 0;
    for (auto it = m_channels.constBegin(); it != m_channels.constEnd(); ++it) {
        if (it.value()->isRunning()) {
            ++count;
        }
    }
    return count;
}

int ChannelManager::getChannelCount() const {
    return m_channels.size();
}

bool ChannelManager::hasRunningChannels() const {
    for (auto it = m_channels.constBegin(); it != m_channels.constEnd(); ++it) {
        if (it.value()->isRunning()) {
            return true;
        }
    }
    return false;
}

QJsonObject ChannelManager::getGlobalStatistics() const {
    QJsonObject stats;
    stats["totalChannels"] = getChannelCount();
    stats["runningChannels"] = getRunningChannelCount();
    
    QJsonArray channelStats;
    for (auto it = m_channels.constBegin(); it != m_channels.constEnd(); ++it) {
        channelStats.append(it.value()->getStatistics());
    }
    stats["channels"] = channelStats;
    
    return stats;
}

void ChannelManager::onChannelStateChanged(ChannelState state) {
    Channel* channel = qobject_cast<Channel*>(sender());
    if (channel) {
        emit channelStateChanged(channel->getName(), state);
        emit statisticsUpdated(getGlobalStatistics());
    }
}

void ChannelManager::onChannelError(const QString& error) {
    Channel* channel = qobject_cast<Channel*>(sender());
    if (channel) {
        emit channelError(channel->getName(), error);
    }
}

// ChannelManagerCore implementation
ChannelManagerCore::ChannelManagerCore(QObject *parent)
    : ChannelManager(parent)
    , m_statusTimer(nullptr)
    , m_initialized(false)
{
    // 创建状态更新定时器
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(1000); // 每秒更新一次
    connect(m_statusTimer, &QTimer::timeout, this, &ChannelManagerCore::updateStatus);
}

ChannelManagerCore::~ChannelManagerCore() {
    cleanup();
}

void ChannelManagerCore::moveToThread(QThread* thread) {
    QObject::moveToThread(thread);
    
    // 移动所有子对象到工作线程
    for (Channel* channel : getAllChannels()) {
        channel->moveToThread(thread);
    }
}

bool ChannelManagerCore::initialize() {
    if (m_initialized) {
        return true;
    }
    
    // 启动状态更新定时器
    m_statusTimer->start();
    
    m_initialized = true;
    return true;
}

void ChannelManagerCore::cleanup() {
    if (!m_initialized) {
        return;
    }
    
    // 停止状态更新定时器
    m_statusTimer->stop();
    
    // 停止所有通道
    stopAll();
    
    m_initialized = false;
}

void ChannelManagerCore::handleCommand(const QString& command, const QVariant& data) {
    bool success = false;
    QVariant responseData;
    
    if (command == "loadConfig") {
        QString configFile = data.toString();
        success = loadConfig(configFile);
        responseData = getConfig();
    } else if (command == "saveConfig") {
        QString configFile = data.toString();
        success = saveConfig(configFile);
    } else if (command == "startChannel") {
        QString channelName = data.toString();
        success = startChannel(channelName);
    } else if (command == "stopChannel") {
        QString channelName = data.toString();
        success = stopChannel(channelName);
    } else if (command == "restartChannel") {
        QString channelName = data.toString();
        success = restartChannel(channelName);
    } else if (command == "startAll") {
        startAll();
        success = true;
    } else if (command == "stopAll") {
        stopAll();
        success = true;
    } else if (command == "getStatistics") {
        responseData = getGlobalStatistics();
        success = true;
    } else {
        qWarning() << "Unknown command:" << command;
    }
    
    emit commandResponse(command, success, responseData);
}

void ChannelManagerCore::updateStatus() {
    emit statusUpdate(getGlobalStatistics());
}

} // namespace ModbusPlexLink
