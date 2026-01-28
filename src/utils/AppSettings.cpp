#include "AppSettings.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QMutexLocker>

namespace ModbusPlexLink {

AppSettings& AppSettings::instance() {
    static AppSettings instance;
    return instance;
}

AppSettings::AppSettings()
    : m_httpPort(8080)
    , m_wsPort(8081)
    , m_apiEnabled(true)
    , m_authEnabled(false)
    , m_authUsername("")
    , m_authPassword("")
    , m_autoStartChannels(false)
    , m_minimizeToTray(true)
    , m_defaultConfigFile("config.json")
    , m_alarmConfigFile("alarm_config.json")
{
    // 尝试自动加载配置
    load();
}

QString AppSettings::settingsFilePath() {
    // 优先使用应用程序所在目录
    QString appDir = QCoreApplication::applicationDirPath();
    QString settingsPath = appDir + "/app_settings.json";
    
    // 如果存在则使用，否则使用当前工作目录
    if (QFile::exists(settingsPath)) {
        return settingsPath;
    }
    
    // 检查当前目录
    if (QFile::exists("app_settings.json")) {
        return "app_settings.json";
    }
    
    // 默认返回当前目录下的配置文件
    return "app_settings.json";
}

void AppSettings::resetToDefaults() {
    QMutexLocker locker(&m_mutex);
    
    m_httpPort = 8080;
    m_wsPort = 8081;
    m_apiEnabled = true;
    m_authEnabled = false;
    m_authUsername = "";
    m_authPassword = "";
    m_autoStartChannels = false;
    m_minimizeToTray = true;
    m_defaultConfigFile = "config.json";
    m_alarmConfigFile = "alarm_config.json";
}

bool AppSettings::load(const QString& filename) {
    QMutexLocker locker(&m_mutex);
    
    QString filePath = filename.isEmpty() ? settingsFilePath() : filename;
    
    QFile file(filePath);
    if (!file.exists()) {
        qDebug() << "[AppSettings] 配置文件不存在，使用默认值:" << filePath;
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[AppSettings] 无法打开配置文件:" << filePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[AppSettings] JSON解析错误:" << parseError.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // API服务器设置
    if (root.contains("api")) {
        QJsonObject api = root["api"].toObject();
        m_httpPort = static_cast<quint16>(api.value("httpPort").toInt(8080));
        m_wsPort = static_cast<quint16>(api.value("wsPort").toInt(8081));
        m_apiEnabled = api.value("enabled").toBool(true);
    }
    
    // 认证设置
    if (root.contains("auth")) {
        QJsonObject auth = root["auth"].toObject();
        m_authEnabled = auth.value("enabled").toBool(false);
        m_authUsername = auth.value("username").toString();
        m_authPassword = auth.value("password").toString();
    }
    
    // 启动设置
    if (root.contains("startup")) {
        QJsonObject startup = root["startup"].toObject();
        m_autoStartChannels = startup.value("autoStartChannels").toBool(false);
        m_minimizeToTray = startup.value("minimizeToTray").toBool(true);
    }
    
    // 配置文件路径
    if (root.contains("files")) {
        QJsonObject files = root["files"].toObject();
        m_defaultConfigFile = files.value("channelConfig").toString("config.json");
        m_alarmConfigFile = files.value("alarmConfig").toString("alarm_config.json");
    }
    
    qInfo() << "[AppSettings] 配置已加载:" << filePath;
    qInfo() << "  - HTTP端口:" << m_httpPort;
    qInfo() << "  - WebSocket端口:" << m_wsPort;
    qInfo() << "  - API启用:" << (m_apiEnabled ? "是" : "否");
    qInfo() << "  - 认证:" << (m_authEnabled ? "启用" : "禁用");
    
    return true;
}

bool AppSettings::save(const QString& filename) const {
    QMutexLocker locker(&m_mutex);
    
    QString filePath = filename.isEmpty() ? "app_settings.json" : filename;
    
    QJsonObject root;
    
    // API服务器设置
    QJsonObject api;
    api["httpPort"] = m_httpPort;
    api["wsPort"] = m_wsPort;
    api["enabled"] = m_apiEnabled;
    root["api"] = api;
    
    // 认证设置
    QJsonObject auth;
    auth["enabled"] = m_authEnabled;
    auth["username"] = m_authUsername;
    auth["password"] = m_authPassword;
    root["auth"] = auth;
    
    // 启动设置
    QJsonObject startup;
    startup["autoStartChannels"] = m_autoStartChannels;
    startup["minimizeToTray"] = m_minimizeToTray;
    root["startup"] = startup;
    
    // 配置文件路径
    QJsonObject files;
    files["channelConfig"] = m_defaultConfigFile;
    files["alarmConfig"] = m_alarmConfigFile;
    root["files"] = files;
    
    // 写入文件
    QJsonDocument doc(root);
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[AppSettings] 无法写入配置文件:" << filePath;
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qInfo() << "[AppSettings] 配置已保存:" << filePath;
    return true;
}

} // namespace ModbusPlexLink
