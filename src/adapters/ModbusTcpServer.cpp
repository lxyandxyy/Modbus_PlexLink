#include "ModbusTcpServer.h"
#include "core/UniversalDataModel.h"
#include <QDebug>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QDateTime>
#include <QtConcurrent/QtConcurrent>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace ModbusPlexLink {

ModbusTcpServer::ModbusTcpServer(const QString& name, QObject *parent)
    : IServer(parent)
    , m_name(name)
    , m_running(false)
    , m_listenAddress("0.0.0.0")
    , m_listenPort(502)
    , m_serverContext(nullptr)
    , m_mapping(nullptr)
    , m_socket(-1)
    , m_maxClients(10)
    , m_udm(nullptr)
    , m_virtualizationEngine(nullptr)
    , m_serverThread(nullptr)
    , m_stopRequested(false)
    , m_timeout(1000)
    , m_maxRequestSize(260)
    , m_logRequests(false)
    , m_logErrors(true)
{
    // 初始化统计信息
    memset(&m_stats, 0, sizeof(m_stats));
    
    // 创建虚拟化引擎
    m_virtualizationEngine = new VirtualizationEngine(this);
}

ModbusTcpServer::~ModbusTcpServer() {
    stop();
    
    if (m_serverThread) {
        m_serverThread->quit();
        m_serverThread->wait();
        delete m_serverThread;
    }
}

bool ModbusTcpServer::initialize(const QJsonObject& config) {
    QMutexLocker locker(&m_mutex);
    
    // 解析配置
    m_listenAddress = config["listenAddress"].toString("0.0.0.0");
    m_listenPort = config["port"].toInt(502);
    m_timeout = config["timeout"].toInt(1000);
    m_maxClients = config["maxClients"].toInt(10);
    m_logRequests = config["logRequests"].toBool(false);
    m_logErrors = config["logErrors"].toBool(true);
    
    // 验证配置
    if (m_listenPort <= 0 || m_listenPort > 65535) {
        qWarning() << "ModbusTcpServer" << m_name << ": Invalid port" << m_listenPort;
        return false;
    }
    
    qInfo() << "ModbusTcpServer" << m_name << "initialized:"
            << "Address=" << m_listenAddress
            << "Port=" << m_listenPort
            << "MaxClients=" << m_maxClients;
    
    return true;
}

void ModbusTcpServer::setDataModel(UniversalDataModel* udm) {
    QMutexLocker locker(&m_mutex);
    m_udm = udm;
}

void ModbusTcpServer::setVirtualizationEngine(VirtualizationEngine* engine) {
    QMutexLocker locker(&m_mutex);
    m_virtualizationEngine = engine;
}

void ModbusTcpServer::setVirtualizationRules(const QJsonObject& rules) {
    QMutexLocker locker(&m_mutex);
    m_virtualizationRules = rules;
    
    if (m_virtualizationEngine) {
        m_virtualizationEngine->setRules(rules);
    }
}

void ModbusTcpServer::setVirtualDevices(const QList<VirtualDevice>& devices) {
    QMutexLocker locker(&m_mutex);
    m_virtualDevices = devices;
    
    if (m_virtualizationEngine) {
        m_virtualizationEngine->setVirtualDevices(devices);
    }
    
    qDebug() << "ModbusTcpServer" << m_name 
             << ": Set" << devices.size() << "virtual devices";
}

bool ModbusTcpServer::start() {
    {
        QMutexLocker locker(&m_mutex);
        
        if (m_running) {
            return true;
        }
        
        m_running = true;
        m_stopRequested = false;
        m_stats.serverStartTime = QDateTime::currentMSecsSinceEpoch();
    }
    
    qInfo() << "ModbusTcpServer" << m_name << ": Starting server in background...";
    
    // 在后台线程启动服务器（避免阻塞UI）
    QtConcurrent::run([this]() {
        // 创建Modbus服务器上下文
        modbus_t* serverContext = modbus_new_tcp(m_listenAddress.toStdString().c_str(), m_listenPort);
        if (!serverContext) {
            qWarning() << "ModbusTcpServer" << m_name 
                       << ": Failed to create server context";
            
            QMutexLocker locker(&m_mutex);
            m_running = false;
            return;
        }
        
        // 设置调试模式（如果需要）
        if (m_logRequests) {
            modbus_set_debug(serverContext, TRUE);
        }
        
        // 创建内存映射（最大尺寸）
        modbus_mapping_t* mapping = modbus_mapping_new(10000, 10000, 10000, 10000);
        if (!mapping) {
            qWarning() << "ModbusTcpServer" << m_name 
                       << ": Failed to create modbus mapping";
            modbus_free(serverContext);
            
            QMutexLocker locker(&m_mutex);
            m_running = false;
            return;
        }
        
        // 监听连接（这是阻塞操作，在后台线程执行）
        int socket = modbus_tcp_listen(serverContext, m_maxClients);
        if (socket == -1) {
            qWarning() << "ModbusTcpServer" << m_name 
                       << ": Failed to listen on" << m_listenAddress << ":" << m_listenPort
                       << "Error:" << modbus_strerror(errno);
            modbus_mapping_free(mapping);
            modbus_free(serverContext);
            
            QMutexLocker locker(&m_mutex);
            m_running = false;
            return;
        }
        
        // 快速保存上下文（在锁内）
        {
            QMutexLocker locker(&m_mutex);
            
            // 再次检查是否已停止（可能在启动期间被停止）
            if (!m_running) {
                modbus_close(serverContext);
                modbus_mapping_free(mapping);
                modbus_free(serverContext);
                return;
            }
            
            m_serverContext = serverContext;
            m_mapping = mapping;
            m_socket = socket;
        }
        
        qInfo() << "ModbusTcpServer" << m_name 
                << ": ✓ Server started on" << m_listenAddress << ":" << m_listenPort;
        
        // 直接在后台线程运行服务器循环（避免阻塞UI）
        serverLoop();
    });
    
    return true;
}

void ModbusTcpServer::stop() {
    {
        QMutexLocker locker(&m_mutex);
        
        if (!m_running) {
            return;
        }
        
        m_running = false;
        m_stopRequested = true;
    }
    
    qInfo() << "ModbusTcpServer" << m_name << ": Stopping server (serverLoop will exit)...";
    
    // 发送停止信号（serverLoop会检测m_stopRequested并退出）
    emit stopRequested();
    
    // 同步等待serverLoop退出（最多等待1秒）
    QThread::msleep(500);
    
    qDebug() << "ModbusTcpServer" << m_name << ": Cleaning up resources...";
    
    // 清理资源（同步执行，确保在对象销毁前完成）
    {
        QMutexLocker locker(&m_mutex);
        
        // 关闭所有客户端连接
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            close(it.key());
        }
        m_clients.clear();
        
        // 关闭服务器socket
        if (m_socket != -1) {
            close(m_socket);
            m_socket = -1;
        }
        
        // 释放Modbus资源
        if (m_mapping) {
            modbus_mapping_free(m_mapping);
            m_mapping = nullptr;
        }
        
        if (m_serverContext) {
            modbus_close(m_serverContext);
            modbus_free(m_serverContext);
            m_serverContext = nullptr;
        }
    }
    
    qInfo() << "ModbusTcpServer" << m_name << ": ✓ Server stopped";
}

bool ModbusTcpServer::isRunning() const {
    QMutexLocker locker(&m_mutex);
    return m_running;
}

QString ModbusTcpServer::getName() const {
    return m_name;
}

QString ModbusTcpServer::getListenAddress() const {
    QMutexLocker locker(&m_mutex);
    return m_listenAddress;
}

int ModbusTcpServer::getListenPort() const {
    QMutexLocker locker(&m_mutex);
    return m_listenPort;
}

int ModbusTcpServer::getClientCount() const {
    QMutexLocker locker(&m_mutex);
    return m_clients.size();
}

QJsonObject ModbusTcpServer::getStatistics() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject stats;
    stats["name"] = m_name;
    stats["running"] = m_running;
    stats["address"] = m_listenAddress;
    stats["port"] = m_listenPort;
    stats["clientCount"] = m_clients.size();
    stats["totalRequests"] = static_cast<qint64>(m_stats.totalRequests);
    stats["successfulRequests"] = static_cast<qint64>(m_stats.successfulRequests);
    stats["failedRequests"] = static_cast<qint64>(m_stats.failedRequests);
    stats["bytesReceived"] = static_cast<qint64>(m_stats.bytesReceived);
    stats["bytesSent"] = static_cast<qint64>(m_stats.bytesSent);
    stats["averageResponseTime"] = m_stats.averageResponseTime;
    
    if (m_running && m_stats.serverStartTime > 0) {
        qint64 uptime = QDateTime::currentMSecsSinceEpoch() - m_stats.serverStartTime;
        stats["uptimeSeconds"] = uptime / 1000;
    }
    
    return stats;
}

void ModbusTcpServer::serverLoop() {
    fd_set readfds;
    int fdmax = m_socket;
    
    while (!m_stopRequested && m_running) {
        FD_ZERO(&readfds);
        FD_SET(m_socket, &readfds);
        
        // 添加客户端socket到集合
        QList<int> clientSockets;
        {
            QMutexLocker locker(&m_mutex);
            for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
                FD_SET(it.key(), &readfds);
                if (it.key() > fdmax) {
                    fdmax = it.key();
                }
                clientSockets.append(it.key());
            }
        }
        
        // 设置超时
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        
        // 等待事件
        int activity = select(fdmax + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            if (errno != EINTR) {
                qWarning() << "ModbusTcpServer" << m_name 
                          << ": select error:" << strerror(errno);
            }
            continue;
        }
        
        if (activity == 0) {
            // 超时，继续循环
            continue;
        }
        
        // 检查新连接
        if (FD_ISSET(m_socket, &readfds)) {
            int newSocket = modbus_tcp_accept(m_serverContext, &m_socket);
            if (newSocket != -1) {
                QMutexLocker locker(&m_mutex);
                
                if (m_clients.size() < m_maxClients) {
                    ClientInfo client;
                    client.socket = newSocket;
                    client.connectTime = QDateTime::currentMSecsSinceEpoch();
                    client.requestCount = 0;
                    
                    // 获取客户端地址
                    struct sockaddr_in addr;
                    socklen_t addrlen = sizeof(addr);
                    if (getpeername(newSocket, (struct sockaddr*)&addr, &addrlen) == 0) {
                        char ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
                        client.address = QString::fromLatin1(ip);
                    }
                    
                    m_clients[newSocket] = client;
                    
                    qInfo() << "ModbusTcpServer" << m_name 
                           << ": Client connected from" << client.address;
                    
                    emit clientConnected(client.address);
                } else {
                    close(newSocket);
                    qWarning() << "ModbusTcpServer" << m_name 
                              << ": Max clients reached, connection rejected";
                }
            }
        }
        
        // 检查客户端请求
        for (int clientSocket : clientSockets) {
            if (FD_ISSET(clientSocket, &readfds)) {
                uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
                
                // 设置socket和超时（避免阻塞）
                modbus_set_socket(m_serverContext, clientSocket);
                
                struct timeval recv_timeout;
                recv_timeout.tv_sec = 0;
                recv_timeout.tv_usec = 500000;  // 500ms超时
                modbus_set_response_timeout(m_serverContext, recv_timeout.tv_sec, recv_timeout.tv_usec);
                
                // 接收请求（带超时，避免阻塞）
                int rc = modbus_receive(m_serverContext, query);
                
                if (rc > 0) {
                    QElapsedTimer responseTimer;
                    responseTimer.start();
                    
                    // 处理请求
                    handleModbusRequest(m_serverContext, query, rc);
                    
                    // 更新统计
                    {
                        QMutexLocker locker(&m_mutex);
                        if (m_clients.contains(clientSocket)) {
                            m_clients[clientSocket].requestCount++;
                        }
                        
                        m_stats.totalRequests++;
                        m_stats.bytesReceived += rc;
                        
                        // 更新平均响应时间
                        qint64 responseTime = responseTimer.elapsed();
                        if (m_stats.totalRequests > 1) {
                            m_stats.averageResponseTime = 
                                (m_stats.averageResponseTime * (m_stats.totalRequests - 1) + responseTime) 
                                / m_stats.totalRequests;
                        } else {
                            m_stats.averageResponseTime = responseTime;
                        }
                    }
                    
                    if (m_logRequests) {
                        qDebug() << "ModbusTcpServer" << m_name 
                                << ": Request processed in" << responseTimer.elapsed() << "ms";
                    }
                } else if (rc == -1) {
                    // 连接关闭或错误
                    QMutexLocker locker(&m_mutex);
                    
                    if (m_clients.contains(clientSocket)) {
                        QString clientAddr = m_clients[clientSocket].address;
                        m_clients.remove(clientSocket);
                        close(clientSocket);
                        
                        qInfo() << "ModbusTcpServer" << m_name 
                               << ": Client disconnected:" << clientAddr;
                        
                        emit clientDisconnected(clientAddr);
                    }
                }
            }
        }
    }
}

void ModbusTcpServer::handleModbusRequest(modbus_t* ctx, const uint8_t* req, int req_length) {
    if (!ctx || !req || req_length < 1 || !m_udm) {
        return;
    }
    
    int offset = modbus_get_header_length(ctx);
    int function = req[offset];
    int address = (req[offset + 1] << 8) | req[offset + 2];
    
    QVector<quint16> registers;
    QVector<quint8> bits;
    uint8_t rsp[MODBUS_TCP_MAX_ADU_LENGTH];
    int rsp_length = 0;
    bool success = false;
    
    switch (function) {
        case MODBUS_FC_READ_COILS: {
            int count = (req[offset + 3] << 8) | req[offset + 4];
            if (readCoilsFromUDM(1, address, count, bits)) {
                // 构建响应
                rsp_length = modbus_reply(ctx, req, req_length, m_mapping);
                success = true;
            }
            break;
        }
        
        case MODBUS_FC_READ_DISCRETE_INPUTS: {
            int count = (req[offset + 3] << 8) | req[offset + 4];
            if (readCoilsFromUDM(1, address, count, bits)) {
                rsp_length = modbus_reply(ctx, req, req_length, m_mapping);
                success = true;
            }
            break;
        }
        
        case MODBUS_FC_READ_HOLDING_REGISTERS: {
            int count = (req[offset + 3] << 8) | req[offset + 4];
            if (readFromUDM(1, RegisterType::HoldingRegister, address, count, registers)) {
                // 填充映射
                for (int i = 0; i < registers.size() && i < count; ++i) {
                    m_mapping->tab_registers[address + i] = registers[i];
                }
                rsp_length = modbus_reply(ctx, req, req_length, m_mapping);
                success = true;
            }
            break;
        }
        
        case MODBUS_FC_READ_INPUT_REGISTERS: {
            int count = (req[offset + 3] << 8) | req[offset + 4];
            if (readFromUDM(1, RegisterType::InputRegister, address, count, registers)) {
                // 填充映射
                for (int i = 0; i < registers.size() && i < count; ++i) {
                    m_mapping->tab_input_registers[address + i] = registers[i];
                }
                rsp_length = modbus_reply(ctx, req, req_length, m_mapping);
                success = true;
            }
            break;
        }
        
        case MODBUS_FC_WRITE_SINGLE_COIL: {
            // 写单个线圈
            int value = (req[offset + 3] << 8) | req[offset + 4];
            QVector<quint8> coils;
            coils.append(value == 0xFF00 ? 1 : 0);
            
            // 写入UDM
            if (writeCoilsToUDM(1, address, coils)) {
                rsp_length = modbus_reply(ctx, req, req_length, m_mapping);
                success = true;
            } else {
                rsp_length = modbus_reply_exception(ctx, req, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
            }
            break;
        }
        
        case MODBUS_FC_WRITE_SINGLE_REGISTER: {
            // 写单个寄存器
            quint16 value = (req[offset + 3] << 8) | req[offset + 4];
            QVector<quint16> registers;
            registers.append(value);
            
            // 写入UDM
            if (writeToUDM(1, RegisterType::HoldingRegister, address, registers)) {
                rsp_length = modbus_reply(ctx, req, req_length, m_mapping);
                success = true;
            } else {
                rsp_length = modbus_reply_exception(ctx, req, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
            }
            break;
        }
        
        case MODBUS_FC_WRITE_MULTIPLE_COILS: {
            // 写多个线圈
            int count = (req[offset + 3] << 8) | req[offset + 4];
            int byteCount = req[offset + 5];
            
            QVector<quint8> coils;
            for (int i = 0; i < count; ++i) {
                int byteIndex = i / 8;
                int bitIndex = i % 8;
                if (byteIndex < byteCount) {
                    quint8 byte = req[offset + 6 + byteIndex];
                    coils.append((byte >> bitIndex) & 0x01);
                }
            }
            
            // 写入UDM
            if (writeCoilsToUDM(1, address, coils)) {
                rsp_length = modbus_reply(ctx, req, req_length, m_mapping);
                success = true;
            } else {
                rsp_length = modbus_reply_exception(ctx, req, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
            }
            break;
        }
        
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS: {
            // 写多个寄存器
            int count = (req[offset + 3] << 8) | req[offset + 4];
            int byteCount = req[offset + 5];
            
            QVector<quint16> registers;
            for (int i = 0; i < count; ++i) {
                int dataOffset = offset + 6 + (i * 2);
                quint16 value = (req[dataOffset] << 8) | req[dataOffset + 1];
                registers.append(value);
            }
            
            // 写入UDM
            if (writeToUDM(1, RegisterType::HoldingRegister, address, registers)) {
                rsp_length = modbus_reply(ctx, req, req_length, m_mapping);
                success = true;
            } else {
                rsp_length = modbus_reply_exception(ctx, req, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
            }
            break;
        }
        
        default:
            // 不支持的功能码
            rsp_length = modbus_reply_exception(ctx, req, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
            break;
    }
    
    // 更新统计
    {
        QMutexLocker locker(&m_mutex);
        if (success) {
            m_stats.successfulRequests++;
            m_stats.bytesSent += rsp_length;
        } else {
            m_stats.failedRequests++;
        }
    }
}

bool ModbusTcpServer::readFromUDM(int virtualUnitId, RegisterType type, int address, int count, QVector<quint16>& values) {
    if (!m_udm || !m_virtualizationEngine) {
        return false;
    }
    
    values.clear();
    values.reserve(count);
    
    int currentAddress = address;
    int registersRead = 0;
    
    while (registersRead < count) {
        // 查找当前地址的映射规则
        const ServerMappingRule* mapping = m_virtualizationEngine->findServerMapping(
            virtualUnitId, type, currentAddress);
        
        if (!mapping) {
            // 地址未映射，填充0
            values.append(0);
            currentAddress++;
            registersRead++;
            continue;
        }
        
        // 从UDM读取数据
        DataPoint dp = m_udm->readPoint(mapping->tagName);
        
        if (!dp.isValid()) {
            // 数据无效，填充0
            int regCount = DataTypeUtils::getRegisterCount(mapping->dataType);
            for (int i = 0; i < regCount && registersRead < count; ++i) {
                values.append(0);
                registersRead++;
            }
            currentAddress += regCount;
            continue;
        }
        
        // 使用ServerMappingRule进行反向转换
        QVector<quint16> convertedRegs = mapping->reverseTransform(dp.value);
        
        // 添加转换后的寄存器值
        for (quint16 reg : convertedRegs) {
            if (registersRead < count) {
                values.append(reg);
                registersRead++;
            }
        }
        
        currentAddress += convertedRegs.size();
        
        // 发送数据请求信号
        emit dataRequested(mapping->tagName);
    }
    
    return true;
}

bool ModbusTcpServer::readCoilsFromUDM(int virtualUnitId, int address, int count, QVector<quint8>& values) {
    if (!m_udm || !m_virtualizationEngine) {
        return false;
    }
    
    values.clear();
    values.reserve(count);
    
    for (int i = 0; i < count; ++i) {
        // 转换地址为标签
        QString tagName = m_virtualizationEngine->translateToTag(virtualUnitId, RegisterType::Coil, address + i);
        
        if (tagName.isEmpty()) {
            values.append(0);
            continue;
        }
        
        // 从UDM读取数据
        DataPoint dp = m_udm->readPoint(tagName);
        
        if (dp.isValid()) {
            bool value = dp.value.toBool();
            values.append(value ? 1 : 0);
        } else {
            values.append(0);
        }
        
        emit dataRequested(tagName);
    }
    
    return true;
}

bool ModbusTcpServer::writeToUDM(int virtualUnitId, RegisterType type, int address, const QVector<quint16>& values) {
    if (!m_udm || !m_virtualizationEngine) {
        return false;
    }
    
    int currentAddress = address;
    int valueOffset = 0;
    
    while (valueOffset < values.size()) {
        // 查找当前地址的映射规则
        const ServerMappingRule* mapping = m_virtualizationEngine->findServerMapping(
            virtualUnitId, type, currentAddress);
        
        if (!mapping) {
            // 地址未映射，跳过
            currentAddress++;
            valueOffset++;
            continue;
        }
        
        // 检查是否可写
        if (!mapping->writable) {
            qWarning() << "ModbusTcpServer" << m_name 
                      << ": Tag" << mapping->tagName << "is not writable";
            int regCount = DataTypeUtils::getRegisterCount(mapping->dataType);
            currentAddress += regCount;
            valueOffset += regCount;
            continue;
        }
        
        // 获取该映射需要的寄存器数量
        int regCount = DataTypeUtils::getRegisterCount(mapping->dataType);
        
        // 检查是否有足够的数据
        if (valueOffset + regCount > values.size()) {
            qWarning() << "ModbusTcpServer" << m_name 
                      << ": Not enough data for tag" << mapping->tagName;
            break;
        }
        
        // 提取寄存器数据
        QVector<quint16> registers;
        for (int i = 0; i < regCount; ++i) {
            registers.append(values[valueOffset + i]);
        }
        
        // 使用DataTypeUtils将寄存器数据转换为实际值（与采集器相同的转换过程）
        QVariant value = DataTypeUtils::parseRegisters(
            registers, mapping->dataType, mapping->byteOrder, mapping->scale, mapping->offset);
        
        if (value.isValid()) {
            // 创建数据点并写入UDM
            DataPoint dp(value, DataQuality::Good);
            m_udm->updatePoint(mapping->tagName, dp);
            
            qDebug() << "ModbusTcpServer" << m_name 
                    << ": Written" << value << "to tag" << mapping->tagName;
            
            // 发送数据写入信号
            emit dataWritten(mapping->tagName, value);
        }
        
        currentAddress += regCount;
        valueOffset += regCount;
    }
    
    return true;
}

bool ModbusTcpServer::writeCoilsToUDM(int virtualUnitId, int address, const QVector<quint8>& values) {
    if (!m_udm || !m_virtualizationEngine) {
        return false;
    }
    
    for (int i = 0; i < values.size(); ++i) {
        // 转换地址为标签
        QString tagName = m_virtualizationEngine->translateToTag(
            virtualUnitId, RegisterType::Coil, address + i);
        
        if (tagName.isEmpty()) {
            continue;
        }
        
        // 查找映射规则检查是否可写
        const ServerMappingRule* mapping = m_virtualizationEngine->findServerMapping(
            virtualUnitId, RegisterType::Coil, address + i);
        
        if (mapping && !mapping->writable) {
            qWarning() << "ModbusTcpServer" << m_name 
                      << ": Coil tag" << tagName << "is not writable";
            continue;
        }
        
        // 创建数据点并写入UDM
        bool value = (values[i] != 0);
        DataPoint dp(value, DataQuality::Good);
        m_udm->updatePoint(tagName, dp);
        
        qDebug() << "ModbusTcpServer" << m_name 
                << ": Written coil" << value << "to tag" << tagName;
        
        // 发送数据写入信号
        emit dataWritten(tagName, value);
    }
    
    return true;
}

QString ModbusTcpServer::translateAddress(int virtualUnitId, RegisterType type, int address) {
    if (m_virtualizationEngine) {
        return m_virtualizationEngine->translateToTag(virtualUnitId, type, address);
    }
    
    // 默认映射（无虚拟化）
    QString typeStr;
    switch (type) {
        case RegisterType::Coil: typeStr = "C"; break;
        case RegisterType::DiscreteInput: typeStr = "DI"; break;
        case RegisterType::HoldingRegister: typeStr = "HR"; break;
        case RegisterType::InputRegister: typeStr = "IR"; break;
    }
    
    return QString("Unit%1.%2.%3").arg(virtualUnitId).arg(typeStr).arg(address);
}

// VirtualizationEngine Implementation

VirtualizationEngine::VirtualizationEngine(QObject *parent)
    : QObject(parent)
{
}

VirtualizationEngine::~VirtualizationEngine() {
}

void VirtualizationEngine::setVirtualDevices(const QList<VirtualDevice>& devices) {
    m_virtualDevices.clear();
    m_addressMap.clear();
    m_tagToAddress.clear();
    
    for (const VirtualDevice& device : devices) {
        if (!device.enabled) {
            continue;
        }
        
        m_virtualDevices[device.virtualUnitId] = device;
    }
    
    // 构建查找表
    buildLookupTables();
    
    qDebug() << "VirtualizationEngine: Loaded" << m_virtualDevices.size() << "virtual devices";
}

void VirtualizationEngine::buildLookupTables() {
    m_addressMap.clear();
    m_tagToAddress.clear();
    
    for (auto it = m_virtualDevices.begin(); it != m_virtualDevices.end(); ++it) {
        int unitId = it.key();
        const VirtualDevice& device = it.value();
        
        for (int i = 0; i < device.mappings.size(); ++i) {
            const ServerMappingRule& mapping = device.mappings[i];
            
            // 构建地址查找表（存储unitId和索引）
            AddressKey key;
            key.unitId = unitId;
            key.regType = mapping.registerType;
            key.address = mapping.address;
            
            m_addressMap[key] = qMakePair(unitId, i);
            
            // 构建标签反向查找表
            m_tagToAddress[mapping.tagName] = qMakePair(unitId, mapping.address);
        }
    }
}

void VirtualizationEngine::setRules(const QJsonObject& rules) {
    // 解析旧格式规则并转换为VirtualDevice
    parseRules(rules);
}

const ServerMappingRule* VirtualizationEngine::findServerMapping(int virtualUnitId, RegisterType type, int address) const {
    AddressKey key;
    key.unitId = virtualUnitId;
    key.regType = type;
    key.address = address;
    
    auto it = m_addressMap.find(key);
    if (it != m_addressMap.end()) {
        int unitId = it.value().first;
        int mappingIndex = it.value().second;
        
        // 从虚拟设备中获取映射规则
        auto devIt = m_virtualDevices.find(unitId);
        if (devIt != m_virtualDevices.end()) {
            const VirtualDevice& device = devIt.value();
            if (mappingIndex >= 0 && mappingIndex < device.mappings.size()) {
                return &device.mappings[mappingIndex];
            }
        }
    }
    
    return nullptr;
}

QString VirtualizationEngine::translateToTag(int virtualUnitId, RegisterType type, int address) const {
    const ServerMappingRule* mapping = findServerMapping(virtualUnitId, type, address);
    if (mapping) {
        return mapping->tagName;
    }
    
    return QString();
}

bool VirtualizationEngine::translateFromTag(const QString& tagName, int& virtualUnitId, RegisterType& type, int& address) const {
    // 直接在虚拟设备中查找标签
    for (auto it = m_virtualDevices.constBegin(); it != m_virtualDevices.constEnd(); ++it) {
        const VirtualDevice& device = it.value();
        
        for (const ServerMappingRule& mapping : device.mappings) {
            if (mapping.tagName == tagName) {
                virtualUnitId = device.virtualUnitId;
                type = mapping.registerType;
                address = mapping.address;
                return true;
            }
        }
    }
    
    return false;
}

QStringList VirtualizationEngine::getTagsForUnit(int virtualUnitId) const {
    QStringList tags;
    
    auto it = m_virtualDevices.find(virtualUnitId);
    if (it != m_virtualDevices.end()) {
        const VirtualDevice& device = it.value();
        for (const ServerMappingRule& mapping : device.mappings) {
            tags.append(mapping.tagName);
        }
    }
    
    return tags;
}

QList<int> VirtualizationEngine::getAllVirtualUnitIds() const {
    return m_virtualDevices.keys();
}

bool VirtualizationEngine::isValidAddress(int virtualUnitId, RegisterType type, int address) const {
    AddressKey key;
    key.unitId = virtualUnitId;
    key.regType = type;
    key.address = address;
    
    return m_addressMap.contains(key);
}

const VirtualDevice* VirtualizationEngine::getVirtualDevice(int virtualUnitId) const {
    auto it = m_virtualDevices.find(virtualUnitId);
    if (it != m_virtualDevices.end()) {
        return &it.value();
    }
    return nullptr;
}

void VirtualizationEngine::parseRules(const QJsonObject& rules) {
    // 将旧格式的虚拟化规则转换为新的VirtualDevice格式
    QJsonArray virtualizations = rules["virtualization"].toArray();
    QList<VirtualDevice> devices;
    
    for (const QJsonValue& value : virtualizations) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject virtObj = value.toObject();
        
        VirtualDevice device;
        device.virtualUnitId = virtObj["virtualUnitId"].toInt();
        device.name = virtObj["name"].toString(QString("Virtual_%1").arg(device.virtualUnitId));
        device.description = virtObj["description"].toString();
        device.enabled = virtObj["enabled"].toBool(true);
        
        QString tagPrefix = virtObj["tagPrefix"].toString();
        
        // 解析地址映射
        QJsonArray addressMap = virtObj["addressMap"].toArray();
        for (const QJsonValue& mapValue : addressMap) {
            if (!mapValue.isObject()) {
                continue;
            }
            
            QJsonObject mapObj = mapValue.toObject();
            
            ServerMappingRule mapping;
            mapping.tagName = tagPrefix + mapObj["tagSuffix"].toString();
            mapping.address = mapObj["address"].toInt();
            mapping.comment = mapObj["comment"].toString();
            
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
            mapping.writable = mapObj["writable"].toBool(false);
            
            device.mappings.append(mapping);
        }
        
        devices.append(device);
    }
    
    // 使用新方法设置虚拟设备
    setVirtualDevices(devices);
}

} // namespace ModbusPlexLink
