#ifndef CHANNEL_H
#define CHANNEL_H

#include <QObject>
#include <QList>
#include <QJsonObject>
#include <memory>
#include "UniversalDataModel.h"
#include "adapters/interfaces.h"

namespace ModbusPlexLink {

// 通道状态枚举
enum class ChannelState {
    Stopped,    // 已停止
    Starting,   // 正在启动
    Running,    // 运行中
    Stopping,   // 正在停止
    Error       // 错误状态
};

// 通道配置结构
struct ChannelConfig {
    QString name;                       // 通道名称
    bool enabled = true;                // 是否启用
    QString description;                // 通道描述
    QList<QJsonObject> collectors;      // 采集器配置列表（包含mappings）
    QList<QJsonObject> servers;         // 服务器配置列表（包含virtualDevices）
    QJsonObject virtualization;         // 虚拟化配置（兼容旧格式）
};

class Channel : public QObject {
    Q_OBJECT
    
public:
    explicit Channel(const QString& name, QObject *parent = nullptr);
    ~Channel();
    
    // 配置通道
    bool configure(const ChannelConfig& config);
    bool configure(const QJsonObject& config);
    
    // 启动通道
    bool start();
    
    // 停止通道
    void stop();
    
    // 重启通道
    bool restart();
    
    // 获取通道名称
    QString getName() const;
    
    // 获取通道状态
    ChannelState getState() const;
    
    // 是否正在运行
    bool isRunning() const;
    
    // 获取UDM实例
    UniversalDataModel* getDataModel();
    const UniversalDataModel* getDataModel() const;
    
    // 获取采集器列表
    QList<ICollector*> getCollectors() const;
    
    // 获取服务器列表
    QList<IServer*> getServers() const;
    
    // 获取指定名称的采集器
    ICollector* getCollector(const QString& name) const;
    
    // 获取指定名称的服务器
    IServer* getServer(const QString& name) const;
    
    // 添加采集器
    bool addCollector(ICollector* collector);
    bool addCollector(const QJsonObject& config);  // 从JSON配置创建并添加
    
    // 添加服务器
    bool addServer(IServer* server);
    bool addServer(const QJsonObject& config);  // 从JSON配置创建并添加
    
    // 移除采集器
    bool removeCollector(const QString& name);
    
    // 移除服务器
    bool removeServer(const QString& name);
    
    // 获取通道配置
    ChannelConfig getConfig() const;
    
    // 获取统计信息
    QJsonObject getStatistics() const;
    
signals:
    // 状态变化信号
    void stateChanged(ChannelState newState);

    // 错误信号
    void errorOccurred(const QString& error);

    // 采集器状态变化
    void collectorStateChanged(const QString& collectorName, bool connected);

    // 服务器客户端变化
    void serverClientChanged(const QString& serverName, int clientCount);

    // 数据更新信号（转发UDM的信号）
    void dataUpdated(const QString& tagName, const DataPoint& point);

    // Modbus报文信号（转发采集器/服务器的报文）
    void modbusMessage(const QString& source, const QString& direction,
                      const QString& device, const QString& function,
                      const QString& address, const QString& data, bool success);
    
private slots:
    // 处理采集器连接状态变化
    void onCollectorConnectionChanged(bool connected);
    
    // 处理采集器错误
    void onCollectorError(const QString& error);
    
    // 处理服务器客户端连接
    void onServerClientConnected(const QString& clientAddress);
    
    // 处理服务器客户端断开
    void onServerClientDisconnected(const QString& clientAddress);
    
    // 处理服务器错误
    void onServerError(const QString& error);
    
private:
    // 创建采集器
    ICollector* createCollector(const QJsonObject& config);
    
    // 创建服务器
    IServer* createServer(const QJsonObject& config);
    
    // 设置状态
    void setState(ChannelState state);
    
    // 清理资源
    void cleanup();
    
private:
    QString m_name;                             // 通道名称
    ChannelState m_state;                       // 通道状态
    std::unique_ptr<UniversalDataModel> m_udm;  // UDM实例
    QList<ICollector*> m_collectors;            // 采集器列表
    QList<IServer*> m_servers;                  // 服务器列表
    ChannelConfig m_config;                     // 通道配置
};

} // namespace ModbusPlexLink

#endif // CHANNEL_H
