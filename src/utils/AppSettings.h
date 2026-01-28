#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>
#include <QJsonObject>
#include <QMutex>

namespace ModbusPlexLink {

/**
 * @brief 应用程序设置管理类
 * 
 * 管理GUI和无头服务的通用设置，包括：
 * - API服务器端口配置
 * - 认证设置
 * - 启动行为设置
 * 
 * 配置文件: app_settings.json
 */
class AppSettings {
public:
    // 获取单例实例
    static AppSettings& instance();
    
    // 禁用拷贝和赋值
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;
    
    // 加载和保存
    bool load(const QString& filename = "app_settings.json");
    bool save(const QString& filename = "app_settings.json") const;
    
    // === API服务器设置 ===
    quint16 httpPort() const { return m_httpPort; }
    void setHttpPort(quint16 port) { m_httpPort = port; }
    
    quint16 wsPort() const { return m_wsPort; }
    void setWsPort(quint16 port) { m_wsPort = port; }
    
    bool apiEnabled() const { return m_apiEnabled; }
    void setApiEnabled(bool enabled) { m_apiEnabled = enabled; }
    
    // === 认证设置 ===
    bool authEnabled() const { return m_authEnabled; }
    void setAuthEnabled(bool enabled) { m_authEnabled = enabled; }
    
    QString authUsername() const { return m_authUsername; }
    void setAuthUsername(const QString& username) { m_authUsername = username; }
    
    QString authPassword() const { return m_authPassword; }
    void setAuthPassword(const QString& password) { m_authPassword = password; }
    
    // === 启动设置 ===
    bool autoStartChannels() const { return m_autoStartChannels; }
    void setAutoStartChannels(bool autoStart) { m_autoStartChannels = autoStart; }
    
    bool minimizeToTray() const { return m_minimizeToTray; }
    void setMinimizeToTray(bool minimize) { m_minimizeToTray = minimize; }
    
    // === 配置文件路径 ===
    QString defaultConfigFile() const { return m_defaultConfigFile; }
    void setDefaultConfigFile(const QString& file) { m_defaultConfigFile = file; }
    
    QString alarmConfigFile() const { return m_alarmConfigFile; }
    void setAlarmConfigFile(const QString& file) { m_alarmConfigFile = file; }
    
    // 获取设置文件路径
    static QString settingsFilePath();
    
    // 重置为默认值
    void resetToDefaults();

private:
    AppSettings();
    ~AppSettings() = default;
    
    // API服务器设置
    quint16 m_httpPort;
    quint16 m_wsPort;
    bool m_apiEnabled;
    
    // 认证设置
    bool m_authEnabled;
    QString m_authUsername;
    QString m_authPassword;
    
    // 启动设置
    bool m_autoStartChannels;
    bool m_minimizeToTray;
    
    // 配置文件路径
    QString m_defaultConfigFile;
    QString m_alarmConfigFile;
    
    mutable QMutex m_mutex;
};

} // namespace ModbusPlexLink

#endif // APPSETTINGS_H
