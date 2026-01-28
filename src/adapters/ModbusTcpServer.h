#ifndef MODBUSTCPSERVER_H
#define MODBUSTCPSERVER_H

#include "interfaces.h"
#include "core/DataTypes.h"
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>
#include <modbus.h>

namespace ModbusPlexLink {

class VirtualizationEngine;

// Modbus TCP服务器实现
class ModbusTcpServer : public IServer {
    Q_OBJECT
    
public:
    explicit ModbusTcpServer(const QString& name, QObject *parent = nullptr);
    ~ModbusTcpServer() override;
    
    // IServer interface
    bool initialize(const QJsonObject& config) override;
    void setDataModel(UniversalDataModel* udm) override;
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    QString getName() const override;
    QString getListenAddress() const override;
    int getListenPort() const override;
    int getClientCount() const override;
    QJsonObject getStatistics() const override;
    
    // 设置虚拟化引擎
    void setVirtualizationEngine(VirtualizationEngine* engine);
    
    // 设置虚拟化规则（已弃用，使用setVirtualDevices）
    void setVirtualizationRules(const QJsonObject& rules);
    
    // 设置虚拟设备列表（新方法）
    void setVirtualDevices(const QList<VirtualDevice>& devices);
    
signals:
    // 内部信号（用于线程间通信）
    void startRequested();
    void stopRequested();
    
    // 数据写入信号（当上层写入数据到服务端时触发）
    void dataWritten(const QString& tagName, const QVariant& value);
    
private slots:
    // 服务器线程主循环
    void serverLoop();
    
private:
    // 处理Modbus请求
    void handleModbusRequest(modbus_t* ctx, const uint8_t* req, int req_length);
    
    // 处理读请求
    void handleReadRequest(modbus_t* ctx, int function, int address, int count);
    
    // 处理写请求
    void handleWriteRequest(modbus_t* ctx, int function, int address, int count, const uint8_t* data);
    
    // 从UDM读取数据
    bool readFromUDM(int virtualUnitId, RegisterType type, int address, int count, QVector<quint16>& values);
    bool readCoilsFromUDM(int virtualUnitId, int address, int count, QVector<quint8>& values);
    
    // 写入数据到UDM
    bool writeToUDM(int virtualUnitId, RegisterType type, int address, const QVector<quint16>& values);
    bool writeCoilsToUDM(int virtualUnitId, int address, const QVector<quint8>& values);
    
    // 地址转换
    QString translateAddress(int virtualUnitId, RegisterType type, int address);
    
    // 更新统计信息
    void updateStatistics(bool success, int bytesTransferred = 0);
    
private:
    // 基本属性
    QString m_name;
    bool m_running;
    QString m_listenAddress;
    int m_listenPort;
    
    // Modbus服务器上下文
    modbus_t* m_serverContext;
    modbus_mapping_t* m_mapping;
    int m_socket;
    
    // 客户端管理
    struct ClientInfo {
        QString address;
        int socket;
        qint64 connectTime;
        quint64 requestCount;
    };
    QMap<int, ClientInfo> m_clients;
    int m_maxClients;
    
    // 数据模型和虚拟化
    UniversalDataModel* m_udm;
    VirtualizationEngine* m_virtualizationEngine;
    QJsonObject m_virtualizationRules;  // 保留用于兼容
    QList<VirtualDevice> m_virtualDevices;  // 虚拟设备列表
    
    // 服务器线程
    QThread* m_serverThread;
    mutable QMutex m_mutex;  // mutable允许在const函数中加锁
    QWaitCondition m_stopCondition;
    bool m_stopRequested;
    
    // 配置参数
    int m_timeout;           // 超时时间（毫秒）
    int m_maxRequestSize;    // 最大请求大小
    bool m_logRequests;      // 记录请求
    bool m_logErrors;        // 记录错误
    
    // 统计信息
    struct Statistics {
        quint64 totalRequests;       // 总请求数
        quint64 successfulRequests;  // 成功请求数
        quint64 failedRequests;      // 失败请求数
        quint64 bytesReceived;       // 接收字节数
        quint64 bytesSent;          // 发送字节数
        qint64 serverStartTime;      // 服务器启动时间
        double averageResponseTime;  // 平均响应时间（毫秒）
    } m_stats;
};

// 虚拟化引擎类
class VirtualizationEngine : public QObject {
    Q_OBJECT
    
public:
    explicit VirtualizationEngine(QObject *parent = nullptr);
    ~VirtualizationEngine();
    
    // 设置虚拟设备列表（新方法）
    void setVirtualDevices(const QList<VirtualDevice>& devices);
    
    // 配置虚拟化规则（已弃用，保留用于兼容）
    void setRules(const QJsonObject& rules);
    
    // 地址转换：虚拟地址 → Tag
    QString translateToTag(int virtualUnitId, RegisterType type, int address) const;
    
    // 根据Tag和虚拟单元ID查找服务端映射规则（返回指针，如果找不到返回nullptr）
    const ServerMappingRule* findServerMapping(int virtualUnitId, RegisterType type, int address) const;
    
    // 反向转换（标签到地址）
    bool translateFromTag(const QString& tagName, int& virtualUnitId, RegisterType& type, int& address) const;
    
    // 获取虚拟单元的所有标签
    QStringList getTagsForUnit(int virtualUnitId) const;
    
    // 获取所有虚拟单元ID
    QList<int> getAllVirtualUnitIds() const;
    
    // 验证地址是否有效
    bool isValidAddress(int virtualUnitId, RegisterType type, int address) const;
    
    // 获取虚拟设备
    const VirtualDevice* getVirtualDevice(int virtualUnitId) const;
    
private:
    // 虚拟设备存储
    QMap<int, VirtualDevice> m_virtualDevices;  // unitId -> VirtualDevice
    
    // 快速查找表：(unitId, regType, address) -> (unitId, mappingIndex)
    struct AddressKey {
        int unitId;
        RegisterType regType;
        int address;
        
        bool operator<(const AddressKey& other) const {
            if (unitId != other.unitId) return unitId < other.unitId;
            if (regType != other.regType) return regType < other.regType;
            return address < other.address;
        }
    };
    
    // 存储(unitId, mappingIndex)而不是指针，避免悬空指针
    QMap<AddressKey, QPair<int, int>> m_addressMap;  // key -> (unitId, mappingIndex)
    
    // 标签到地址的反向映射
    QHash<QString, QPair<int, int>> m_tagToAddress; // tagName -> (unitId, address)
    
    // 解析规则（兼容旧格式）
    void parseRules(const QJsonObject& rules);
    
    // 构建查找表
    void buildLookupTables();
};

// 注意：ModbusServerWorker 已经整合到 ModbusTcpServer 中
// 不再需要单独的工作线程类

} // namespace ModbusPlexLink

#endif // MODBUSTCPSERVER_H
