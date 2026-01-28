#ifndef CHANNELMANAGER_H
#define CHANNELMANAGER_H

#include <QObject>
#include <QMap>
#include <QJsonObject>
#include <QThread>
#include <memory>
#include "Channel.h"

namespace ModbusPlexLink {

class ChannelManager : public QObject {
    Q_OBJECT
    
public:
    explicit ChannelManager(QObject *parent = nullptr);
    ~ChannelManager();
    
    // 加载配置
    bool loadConfig(const QString& configFile);
    bool loadConfig(const QJsonObject& config);
    
    // 保存配置
    bool saveConfig(const QString& configFile) const;
    QJsonObject getConfig() const;
    
    // 创建通道
    Channel* createChannel(const QString& name);
    Channel* createChannel(const ChannelConfig& config);
    
    // 获取通道
    Channel* getChannel(const QString& name);
    const Channel* getChannel(const QString& name) const;
    
    // 获取所有通道
    QList<Channel*> getAllChannels() const;
    QStringList getChannelNames() const;
    
    // 删除通道
    bool deleteChannel(const QString& name);
    
    // 启动指定通道
    bool startChannel(const QString& name);
    
    // 停止指定通道
    bool stopChannel(const QString& name);
    
    // 重启指定通道
    bool restartChannel(const QString& name);
    
    // 启动所有通道
    void startAll();
    
    // 停止所有通道
    void stopAll();
    
    // 获取运行中的通道数
    int getRunningChannelCount() const;
    
    // 获取总通道数
    int getChannelCount() const;
    
    // 检查是否有通道在运行
    bool hasRunningChannels() const;
    
    // 获取全局统计信息
    QJsonObject getGlobalStatistics() const;
    
public slots:
    // 处理通道状态变化
    void onChannelStateChanged(ChannelState state);
    
    // 处理通道错误
    void onChannelError(const QString& error);
    
signals:
    // 通道创建信号
    void channelCreated(const QString& channelName);
    
    // 通道删除信号
    void channelDeleted(const QString& channelName);
    
    // 通道状态变化信号
    void channelStateChanged(const QString& channelName, ChannelState state);
    
    // 通道错误信号
    void channelError(const QString& channelName, const QString& error);
    
    // 配置加载完成信号
    void configLoaded();
    
    // 配置保存完成信号
    void configSaved();
    
    // 全局错误信号
    void globalError(const QString& error);
    
    // 统计更新信号
    void statisticsUpdated(const QJsonObject& stats);
    
private:
    // 通道映射表
    QMap<QString, Channel*> m_channels;
    
    // 配置对象
    QJsonObject m_config;
    
    // 配置文件路径
    QString m_configFile;
};

// 运行在工作线程的通道管理器
class ChannelManagerCore : public ChannelManager {
    Q_OBJECT
    
public:
    explicit ChannelManagerCore(QObject *parent = nullptr);
    ~ChannelManagerCore();
    
    // 移动到工作线程
    void moveToThread(QThread* thread);
    
    // 初始化
    bool initialize();
    
    // 清理
    void cleanup();
    
public slots:
    // 处理控制命令（从UI线程发送的命令）
    void handleCommand(const QString& command, const QVariant& data = QVariant());
    
signals:
    // 响应信号（发送给UI线程）
    void commandResponse(const QString& command, bool success, const QVariant& data = QVariant());
    
    // 状态更新信号（定期发送给UI线程）
    void statusUpdate(const QJsonObject& status);
    
private slots:
    // 定期更新状态
    void updateStatus();
    
private:
    QTimer* m_statusTimer;
    bool m_initialized;
};

} // namespace ModbusPlexLink

#endif // CHANNELMANAGER_H

