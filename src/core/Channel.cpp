#include "Channel.h"
#include "adapters/ModbusTcpCollector.h"
#include "adapters/ModbusRtuCollector.h"
#include "adapters/ModbusTcpServer.h"
#include "core/DataTypes.h"
#include <QDebug>
#include <QJsonArray>

namespace ModbusPlexLink {

Channel::Channel(const QString& name, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_state(ChannelState::Stopped)
    , m_udm(std::make_unique<UniversalDataModel>())
{
    // 连接UDM信号
    connect(m_udm.get(), &UniversalDataModel::dataUpdated,
            this, &Channel::dataUpdated);
}

Channel::~Channel() {
    stop();
    cleanup();
}

bool Channel::configure(const ChannelConfig& config) {
    if (m_state != ChannelState::Stopped) {
        qWarning() << "Channel" << m_name << "must be stopped before configuration";
        return false;
    }
    
    m_config = config;
    
    // 清理旧的采集器和服务器
    cleanup();
    
    // 创建采集器（配置中包含mappings）
    for (const QJsonObject& collectorConfig : config.collectors) {
        ICollector* collector = createCollector(collectorConfig);
        if (collector) {
            addCollector(collector);  // 使用addCollector确保信号正确连接
        }
    }
    
    // 创建服务器（配置中包含virtualDevices）
    for (const QJsonObject& serverConfig : config.servers) {
        IServer* server = createServer(serverConfig);
        if (server) {
            addServer(server);  // 使用addServer确保信号正确连接
        }
    }
    
    return true;
}

bool Channel::configure(const QJsonObject& config) {
    ChannelConfig channelConfig;
    
    // 解析通道名称
    channelConfig.name = config["name"].toString(m_name);
    
    // 解析是否启用
    channelConfig.enabled = config["enabled"].toBool(true);
    
    // 解析通道描述
    channelConfig.description = config["description"].toString();
    
    // 解析采集器配置（包含mappings）
    QJsonArray collectorsArray = config["collectors"].toArray();
    for (const QJsonValue& value : collectorsArray) {
        if (value.isObject()) {
            channelConfig.collectors.append(value.toObject());
        }
    }
    
    // 解析服务器配置（包含virtualDevices）
    QJsonArray serversArray = config["servers"].toArray();
    for (const QJsonValue& value : serversArray) {
        if (value.isObject()) {
            channelConfig.servers.append(value.toObject());
        }
    }
    
    // 解析虚拟化配置（兼容旧格式）
    channelConfig.virtualization = config["virtualization"].toObject();
    
    return configure(channelConfig);
}

bool Channel::start() {
    if (m_state == ChannelState::Running) {
        qInfo() << "Channel" << m_name << "is already running";
        return true;
    }
    
    if (m_state != ChannelState::Stopped) {
        qWarning() << "Channel" << m_name << "is in invalid state for starting:" << static_cast<int>(m_state);
        return false;
    }
    
    setState(ChannelState::Starting);
    
    bool allStarted = true;
    
    // 启动所有采集器
    for (ICollector* collector : m_collectors) {
        if (!collector->start()) {
            qWarning() << "Failed to start collector" << collector->getName() << "in channel" << m_name;
            allStarted = false;
        }
    }
    
    // 启动所有服务器
    for (IServer* server : m_servers) {
        if (!server->start()) {
            qWarning() << "Failed to start server" << server->getName() << "in channel" << m_name;
            allStarted = false;
        }
    }
    
    if (allStarted) {
        setState(ChannelState::Running);
        qInfo() << "Channel" << m_name << "started successfully";
    } else {
        setState(ChannelState::Error);
        emit errorOccurred(QString("Channel %1 failed to start some components").arg(m_name));
    }
    
    return allStarted;
}

void Channel::stop() {
    if (m_state == ChannelState::Stopped) {
        return;
    }
    
    setState(ChannelState::Stopping);
    
    // 停止所有采集器
    for (ICollector* collector : m_collectors) {
        collector->stop();
    }
    
    // 停止所有服务器
    for (IServer* server : m_servers) {
        server->stop();
    }
    
    setState(ChannelState::Stopped);
    qInfo() << "Channel" << m_name << "stopped";
}

bool Channel::restart() {
    stop();
    return start();
}

QString Channel::getName() const {
    return m_name;
}

ChannelState Channel::getState() const {
    return m_state;
}

bool Channel::isRunning() const {
    return m_state == ChannelState::Running;
}

UniversalDataModel* Channel::getDataModel() {
    return m_udm.get();
}

const UniversalDataModel* Channel::getDataModel() const {
    return m_udm.get();
}

QList<ICollector*> Channel::getCollectors() const {
    return m_collectors;
}

QList<IServer*> Channel::getServers() const {
    return m_servers;
}

ICollector* Channel::getCollector(const QString& name) const {
    for (ICollector* collector : m_collectors) {
        if (collector->getName() == name) {
            return collector;
        }
    }
    return nullptr;
}

IServer* Channel::getServer(const QString& name) const {
    for (IServer* server : m_servers) {
        if (server->getName() == name) {
            return server;
        }
    }
    return nullptr;
}

bool Channel::addCollector(ICollector* collector) {
    if (!collector) {
        return false;
    }
    
    if (getCollector(collector->getName())) {
        qWarning() << "Collector" << collector->getName() << "already exists in channel" << m_name;
        return false;
    }
    
    collector->setParent(this);
    collector->setDataModel(m_udm.get());
    
    // 连接信号
    connect(collector, &ICollector::connectionStateChanged,
            this, &Channel::onCollectorConnectionChanged);
    connect(collector, &ICollector::errorOccurred,
            this, &Channel::onCollectorError);
    connect(collector, &ICollector::modbusMessage,
            this, [this, collector](const QString& direction, const QString& device,
                                   const QString& function, const QString& address,
                                   const QString& data, bool success) {
                emit modbusMessage(collector->getName(), direction, device,
                                 function, address, data, success);
            });
    
    m_collectors.append(collector);
    
    // 如果通道正在运行，启动新的采集器
    if (m_state == ChannelState::Running) {
        collector->start();
    }
    
    return true;
}

bool Channel::addServer(IServer* server) {
    if (!server) {
        return false;
    }
    
    if (getServer(server->getName())) {
        qWarning() << "Server" << server->getName() << "already exists in channel" << m_name;
        return false;
    }
    
    server->setParent(this);
    server->setDataModel(m_udm.get());
    
    // 连接信号
    connect(server, &IServer::clientConnected,
            this, &Channel::onServerClientConnected);
    connect(server, &IServer::clientDisconnected,
            this, &Channel::onServerClientDisconnected);
    connect(server, &IServer::errorOccurred,
            this, &Channel::onServerError);
    connect(server, &IServer::modbusMessage,
            this, [this, server](const QString& direction, const QString& device,
                                const QString& function, const QString& address,
                                const QString& data, bool success) {
                emit modbusMessage(server->getName(), direction, device,
                                 function, address, data, success);
            });
    
    m_servers.append(server);
    
    // 如果通道正在运行，启动新的服务器
    if (m_state == ChannelState::Running) {
        server->start();
    }
    
    return true;
}

bool Channel::addCollector(const QJsonObject& config) {
    ICollector* collector = createCollector(config);
    if (collector) {
        return addCollector(collector);
    }
    return false;
}

bool Channel::addServer(const QJsonObject& config) {
    IServer* server = createServer(config);
    if (server) {
        return addServer(server);
    }
    return false;
}

bool Channel::removeCollector(const QString& name) {
    for (int i = 0; i < m_collectors.size(); ++i) {
        if (m_collectors[i]->getName() == name) {
            ICollector* collector = m_collectors.takeAt(i);
            collector->stop();
            collector->deleteLater();
            return true;
        }
    }
    return false;
}

bool Channel::removeServer(const QString& name) {
    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers[i]->getName() == name) {
            IServer* server = m_servers.takeAt(i);
            server->stop();
            server->deleteLater();
            return true;
        }
    }
    return false;
}

ChannelConfig Channel::getConfig() const {
    return m_config;
}

QJsonObject Channel::getStatistics() const {
    QJsonObject stats;
    stats["name"] = m_name;
    stats["state"] = static_cast<int>(m_state);
    stats["dataPointCount"] = m_udm->size();
    
    // 采集器统计
    QJsonArray collectorStats;
    for (ICollector* collector : m_collectors) {
        QJsonObject collectorStat = collector->getStatistics();
        collectorStat["name"] = collector->getName();
        collectorStat["connected"] = collector->isConnected();
        collectorStats.append(collectorStat);
    }
    stats["collectors"] = collectorStats;
    
    // 服务器统计
    QJsonArray serverStats;
    for (IServer* server : m_servers) {
        QJsonObject serverStat = server->getStatistics();
        serverStat["name"] = server->getName();
        serverStat["clientCount"] = server->getClientCount();
        serverStats.append(serverStat);
    }
    stats["servers"] = serverStats;
    
    return stats;
}

void Channel::onCollectorConnectionChanged(bool connected) {
    ICollector* collector = qobject_cast<ICollector*>(sender());
    if (collector) {
        emit collectorStateChanged(collector->getName(), connected);
    }
}

void Channel::onCollectorError(const QString& error) {
    ICollector* collector = qobject_cast<ICollector*>(sender());
    if (collector) {
        QString fullError = QString("Collector %1: %2").arg(collector->getName(), error);
        emit errorOccurred(fullError);
    }
}

void Channel::onServerClientConnected(const QString& clientAddress) {
    IServer* server = qobject_cast<IServer*>(sender());
    if (server) {
        emit serverClientChanged(server->getName(), server->getClientCount());
        qDebug() << "Client" << clientAddress << "connected to server" 
                 << server->getName() << "in channel" << m_name;
    }
}

void Channel::onServerClientDisconnected(const QString& clientAddress) {
    IServer* server = qobject_cast<IServer*>(sender());
    if (server) {
        emit serverClientChanged(server->getName(), server->getClientCount());
        qDebug() << "Client" << clientAddress << "disconnected from server" 
                 << server->getName() << "in channel" << m_name;
    }
}

void Channel::onServerError(const QString& error) {
    IServer* server = qobject_cast<IServer*>(sender());
    if (server) {
        QString fullError = QString("Server %1: %2").arg(server->getName(), error);
        emit errorOccurred(fullError);
    }
}

ICollector* Channel::createCollector(const QJsonObject& config) {
    QString protocol = config["protocol"].toString();
    QString name = config["name"].toString();
    
    if (protocol.isEmpty() || name.isEmpty()) {
        qWarning() << "Invalid collector config: missing protocol or name";
        return nullptr;
    }
    
    ICollector* collector = nullptr;
    
    // 解析采集器的映射规则（通用函数）
    auto parseMappingRules = [](const QJsonObject& config) -> QList<CollectorMappingRule> {
        QList<CollectorMappingRule> mappingRules;
        QJsonArray mappingsArray = config["mappings"].toArray();
        
        for (const QJsonValue& value : mappingsArray) {
            if (!value.isObject()) {
                continue;
            }
            
            QJsonObject mappingObj = value.toObject();
            CollectorMappingRule rule;
            
            rule.tagName = mappingObj["tagName"].toString();
            rule.comment = mappingObj["comment"].toString();
            rule.address = mappingObj["address"].toInt();
            rule.count = mappingObj["count"].toInt(0);
            rule.unit = mappingObj["unit"].toString();
            rule.enabled = mappingObj["enabled"].toBool(true);
            
            rule.registerType = DataTypeUtils::registerTypeFromString(
                mappingObj["registerType"].toString("Holding"));
            rule.dataType = DataTypeUtils::dataTypeFromString(
                mappingObj["dataType"].toString("UInt16"));
            rule.byteOrder = DataTypeUtils::byteOrderFromString(
                mappingObj["byteOrder"].toString("AB"));
            
            rule.scale = mappingObj["scale"].toDouble(1.0);
            rule.offset = mappingObj["offset"].toDouble(0.0);
            
            if (rule.count <= 0) {
                rule.count = DataTypeUtils::getRegisterCount(rule.dataType);
            }
            
            if (!rule.tagName.isEmpty()) {
                mappingRules.append(rule);
            }
        }
        return mappingRules;
    };
    
    // 根据协议类型创建相应的采集器
    if (protocol.toLower() == "modbus-tcp" || protocol.toLower() == "modbus_tcp") {
        // 创建Modbus TCP采集器
        ModbusTcpCollector* modbusCollector = new ModbusTcpCollector(name, this);
        
        if (modbusCollector->initialize(config)) {
            modbusCollector->setDataModel(m_udm.get());
            
            QList<CollectorMappingRule> mappingRules = parseMappingRules(config);
            modbusCollector->setMappingRules(mappingRules);
            
            collector = modbusCollector;
            
            connect(modbusCollector, &ICollector::connectionStateChanged,
                    this, &Channel::onCollectorConnectionChanged);
            connect(modbusCollector, &ICollector::errorOccurred,
                    this, &Channel::onCollectorError);
            
            qDebug() << "Created ModbusTcpCollector" << name 
                     << "with" << mappingRules.size() << "mapping rules"
                     << "for channel" << m_name;
        } else {
            qWarning() << "Failed to initialize ModbusTcpCollector" << name;
            delete modbusCollector;
        }
    } 
    else if (protocol.toLower() == "modbus-rtu" || protocol.toLower() == "modbus_rtu") {
        // 创建Modbus RTU采集器
        ModbusRtuCollector* rtuCollector = new ModbusRtuCollector(name, this);
        
        if (rtuCollector->initialize(config)) {
            rtuCollector->setDataModel(m_udm.get());
            
            QList<CollectorMappingRule> mappingRules = parseMappingRules(config);
            rtuCollector->setMappingRules(mappingRules);
            
            collector = rtuCollector;
            
            connect(rtuCollector, &ICollector::connectionStateChanged,
                    this, &Channel::onCollectorConnectionChanged);
            connect(rtuCollector, &ICollector::errorOccurred,
                    this, &Channel::onCollectorError);
            
            qDebug() << "Created ModbusRtuCollector" << name 
                     << "with" << mappingRules.size() << "mapping rules"
                     << "for channel" << m_name;
        } else {
            qWarning() << "Failed to initialize ModbusRtuCollector" << name;
            delete rtuCollector;
        }
    } 
    else {
        qWarning() << "Unknown collector protocol:" << protocol;
    }
    
    return collector;
}

IServer* Channel::createServer(const QJsonObject& config) {
    QString protocol = config["protocol"].toString();
    QString name = config["name"].toString();
    
    if (protocol.isEmpty() || name.isEmpty()) {
        qWarning() << "Invalid server config: missing protocol or name";
        return nullptr;
    }
    
    IServer* server = nullptr;
    
    // 根据协议类型创建相应的服务器
    if (protocol.toLower() == "modbus-tcp" || protocol.toLower() == "modbus_tcp") {
        // 创建Modbus TCP服务器
        ModbusTcpServer* modbusServer = new ModbusTcpServer(name, this);
        
        // 初始化服务器
        if (modbusServer->initialize(config)) {
            modbusServer->setDataModel(m_udm.get());
            
            // 解析虚拟设备配置
            QList<VirtualDevice> virtualDevices;
            QJsonArray devicesArray = config["virtualDevices"].toArray();
            
            for (const QJsonValue& value : devicesArray) {
                if (!value.isObject()) {
                    continue;
                }
                
                QJsonObject deviceObj = value.toObject();
                VirtualDevice device;
                
                device.virtualUnitId = deviceObj["virtualUnitId"].toInt();
                device.name = deviceObj["name"].toString();
                device.description = deviceObj["description"].toString();
                device.enabled = deviceObj["enabled"].toBool(true);
                
                // 解析虚拟设备的映射规则
                QJsonArray mappingsArray = deviceObj["mappings"].toArray();
                for (const QJsonValue& mapValue : mappingsArray) {
                    if (!mapValue.isObject()) {
                        continue;
                    }
                    
                    QJsonObject mapObj = mapValue.toObject();
                    ServerMappingRule mapping;
                    
                    mapping.tagName = mapObj["tagName"].toString();
                    mapping.comment = mapObj["comment"].toString();
                    mapping.address = mapObj["address"].toInt();
                    mapping.writable = mapObj["writable"].toBool(false);
                    mapping.accessLevel = mapObj["accessLevel"].toString();
                    
                    // 解析寄存器类型
                    mapping.registerType = DataTypeUtils::registerTypeFromString(
                        mapObj["registerType"].toString("Holding"));
                    
                    // 解析数据类型
                    mapping.dataType = DataTypeUtils::dataTypeFromString(
                        mapObj["dataType"].toString("UInt16"));
                    
                    // 解析字节序
                    mapping.byteOrder = DataTypeUtils::byteOrderFromString(
                        mapObj["byteOrder"].toString("AB"));
                    
                    mapping.scale = mapObj["scale"].toDouble(1.0);
                    mapping.offset = mapObj["offset"].toDouble(0.0);
                    
                    if (!mapping.tagName.isEmpty()) {
                        device.mappings.append(mapping);
                    }
                }
                
                if (device.virtualUnitId > 0 && !device.mappings.isEmpty()) {
                    virtualDevices.append(device);
                }
            }
            
            // 设置虚拟设备
            modbusServer->setVirtualDevices(virtualDevices);
            
            // 兼容旧格式：如果有virtualization配置也设置
            if (m_config.virtualization.contains("virtualization")) {
                modbusServer->setVirtualizationRules(m_config.virtualization);
            }
            
            server = modbusServer;
            
            // 连接信号
            connect(modbusServer, &IServer::clientConnected,
                    this, &Channel::onServerClientConnected);
            connect(modbusServer, &IServer::clientDisconnected,
                    this, &Channel::onServerClientDisconnected);
            connect(modbusServer, &IServer::errorOccurred,
                    this, &Channel::onServerError);
            
            // 连接服务端的dataWritten信号到所有采集器的onDataUpdated槽
            // 这样只有上层通过服务端写入的数据才会转发到下行设备
            for (ICollector* coll : m_collectors) {
                // 尝试连接到 TCP 采集器
                ModbusTcpCollector* tcpCollector = qobject_cast<ModbusTcpCollector*>(coll);
                if (tcpCollector) {
                    connect(modbusServer, &ModbusTcpServer::dataWritten,
                            tcpCollector, &ModbusTcpCollector::onDataUpdated);
                    qDebug() << "Connected server" << name << "dataWritten signal to TCP collector" 
                            << coll->getName();
                    continue;
                }
                
                // 尝试连接到 RTU 采集器
                ModbusRtuCollector* rtuCollector = qobject_cast<ModbusRtuCollector*>(coll);
                if (rtuCollector) {
                    connect(modbusServer, &ModbusTcpServer::dataWritten,
                            rtuCollector, &ModbusRtuCollector::onDataUpdated);
                    qDebug() << "Connected server" << name << "dataWritten signal to RTU collector" 
                            << coll->getName();
                }
            }
            
            qDebug() << "Created ModbusTcpServer" << name 
                     << "with" << virtualDevices.size() << "virtual devices"
                     << "for channel" << m_name;
        } else {
            qWarning() << "Failed to initialize ModbusTcpServer" << name;
            delete modbusServer;
        }
    } else {
        qWarning() << "Unknown server protocol:" << protocol;
    }
    
    return server;
}

// parseMappings方法已移除
// 映射规则现在直接在采集器和服务器配置中处理

void Channel::setState(ChannelState state) {
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

void Channel::cleanup() {
    // 停止并删除所有采集器
    for (ICollector* collector : m_collectors) {
        collector->stop();
        collector->deleteLater();
    }
    m_collectors.clear();
    
    // 停止并删除所有服务器
    for (IServer* server : m_servers) {
        server->stop();
        server->deleteLater();
    }
    m_servers.clear();
}

} // namespace ModbusPlexLink
