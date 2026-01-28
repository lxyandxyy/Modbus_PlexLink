#ifndef INTERFACES_H
#define INTERFACES_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QList>
#include "core/DataTypes.h"  // 包含新的数据类型定义

namespace ModbusPlexLink {

class UniversalDataModel;

// 注意：RegisterType, DataType, ByteOrder等定义已移到 DataTypes.h
// 注意：CollectorMappingRule, ServerMappingRule, VirtualDevice 已在 DataTypes.h 中定义

// 采集器接口
class ICollector : public QObject {
    Q_OBJECT
    
public:
    explicit ICollector(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ICollector() = default;
    
    // 初始化采集器
    virtual bool initialize(const QJsonObject& config) = 0;
    
    // 设置UDM引用
    virtual void setDataModel(UniversalDataModel* udm) = 0;
    
    // 设置映射规则（使用新的CollectorMappingRule）
    virtual void setMappingRules(const QList<CollectorMappingRule>& rules) = 0;
    
    // 启动采集
    virtual bool start() = 0;
    
    // 停止采集
    virtual void stop() = 0;
    
    // 是否正在运行
    virtual bool isRunning() const = 0;
    
    // 获取采集器名称
    virtual QString getName() const = 0;
    
    // 获取连接状态
    virtual bool isConnected() const = 0;
    
    // 获取统计信息
    virtual QJsonObject getStatistics() const = 0;
    
signals:
    // 连接状态变化
    void connectionStateChanged(bool connected);

    // 错误发生
    void errorOccurred(const QString& error);

    // 数据采集成功
    void dataCollected(const QString& tagName, const QVariant& value);

    // Modbus报文信号（用于报文查看器）
    // direction: "→" (发送) 或 "←" (接收)
    // device: 设备名称或地址
    // function: 功能码描述 (如 "03-读保持寄存器")
    // address: 起始地址
    // data: 数据内容
    // success: 是否成功
    void modbusMessage(const QString& direction, const QString& device,
                      const QString& function, const QString& address,
                      const QString& data, bool success);
};

// 服务器接口
class IServer : public QObject {
    Q_OBJECT
    
public:
    explicit IServer(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IServer() = default;
    
    // 初始化服务器
    virtual bool initialize(const QJsonObject& config) = 0;
    
    // 设置UDM引用
    virtual void setDataModel(UniversalDataModel* udm) = 0;
    
    // 启动服务
    virtual bool start() = 0;
    
    // 停止服务
    virtual void stop() = 0;
    
    // 是否正在运行
    virtual bool isRunning() const = 0;
    
    // 获取服务器名称
    virtual QString getName() const = 0;
    
    // 获取监听地址
    virtual QString getListenAddress() const = 0;
    
    // 获取监听端口
    virtual int getListenPort() const = 0;
    
    // 获取连接客户端数
    virtual int getClientCount() const = 0;
    
    // 获取统计信息
    virtual QJsonObject getStatistics() const = 0;
    
signals:
    // 客户端连接
    void clientConnected(const QString& clientAddress);

    // 客户端断开
    void clientDisconnected(const QString& clientAddress);

    // 错误发生
    void errorOccurred(const QString& error);

    // 数据请求
    void dataRequested(const QString& tagName);

    // Modbus报文信号（用于报文查看器）
    // direction: "→" (发送) 或 "←" (接收)
    // device: 客户端地址
    // function: 功能码描述
    // address: 起始地址
    // data: 数据内容
    // success: 是否成功
    void modbusMessage(const QString& direction, const QString& device,
                      const QString& function, const QString& address,
                      const QString& data, bool success);
};

// 协议适配器接口
class IProtocolAdapter {
public:
    virtual ~IProtocolAdapter() = default;
    
    // 获取适配器名称
    virtual QString getName() const = 0;
    
    // 获取支持的协议
    virtual QString getProtocol() const = 0;
    
    // 创建采集器实例
    virtual ICollector* createCollector(const QString& name, QObject* parent = nullptr) = 0;
    
    // 创建服务器实例
    virtual IServer* createServer(const QString& name, QObject* parent = nullptr) = 0;
};

} // namespace ModbusPlexLink

#endif // INTERFACES_H
