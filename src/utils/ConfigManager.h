#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include "core/DataTypes.h"

namespace ModbusPlexLink {

// 配置管理器类
class ConfigManager : public QObject {
    Q_OBJECT
    
public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();
    
    // 加载配置文件
    bool loadFromFile(const QString& filename);
    
    // 保存配置文件
    bool saveToFile(const QString& filename) const;
    
    // 从JSON对象加载
    bool loadFromJson(const QJsonObject& json);
    
    // 导出为JSON对象
    QJsonObject toJson() const;
    
    // 获取/设置配置
    QJsonObject getConfig() const;
    void setConfig(const QJsonObject& config);
    
    // 验证配置
    bool validateConfig(QString* errorMsg = nullptr) const;
    
    // 获取配置版本
    QString getVersion() const;
    
    // 解析工具方法
    static QList<CollectorMappingRule> parseCollectorMappings(const QJsonArray& mappings);
    static QList<ServerMappingRule> parseServerMappings(const QJsonArray& mappings);
    static QList<VirtualDevice> parseVirtualDevices(const QJsonArray& devices);
    
    // 编码工具方法
    static QJsonArray encodeCollectorMappings(const QList<CollectorMappingRule>& rules);
    static QJsonArray encodeServerMappings(const QList<ServerMappingRule>& rules);
    static QJsonArray encodeVirtualDevices(const QList<VirtualDevice>& devices);
    
    // CSV 导入/导出
    bool importMappingsFromCSV(const QString& filename, QList<CollectorMappingRule>& rules);
    bool exportMappingsToCSV(const QString& filename, const QList<CollectorMappingRule>& rules);
    
    bool importVirtualDevicesFromCSV(const QString& filename, QList<VirtualDevice>& devices);
    bool exportVirtualDevicesToCSV(const QString& filename, const QList<VirtualDevice>& devices);
    
signals:
    // 配置加载完成
    void configLoaded();
    
    // 配置保存完成
    void configSaved();
    
    // 配置验证失败
    void validationFailed(const QString& error);
    
    // 错误信号
    void errorOccurred(const QString& error);
    
private:
    // 验证采集器配置
    bool validateCollector(const QJsonObject& collector, QString* errorMsg) const;
    
    // 验证服务器配置
    bool validateServer(const QJsonObject& server, QString* errorMsg) const;
    
    // 验证映射规则
    bool validateMapping(const QJsonObject& mapping, bool isCollector, QString* errorMsg) const;
    
    // 验证虚拟设备
    bool validateVirtualDevice(const QJsonObject& device, QString* errorMsg) const;
    
private:
    QJsonObject m_config;
    QString m_version;
};

} // namespace ModbusPlexLink

#endif // CONFIGMANAGER_H

