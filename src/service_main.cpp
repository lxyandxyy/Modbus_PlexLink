/**
 * @file service_main.cpp
 * @brief 无头服务入口 - 用于边缘网关/嵌入式设备部署
 * 
 * 用法:
 *   Modbus_PlexLink_Service [options]
 * 
 * 选项:
 *   --config <file>     指定配置文件 (默认: config.json)
 *   --http-port <port>  HTTP API端口 (默认: 8080)
 *   --ws-port <port>    WebSocket端口 (默认: 8081)
 *   --auth <user:pass>  启用认证
 *   --auto-start        自动启动所有通道
 *   --help              显示帮助信息
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <signal.h>

#include "core/ChannelManager.h"
#include "utils/AlarmManager.h"
#include "utils/AppSettings.h"
#include "remote/RemoteApiServer.h"

using namespace ModbusPlexLink;

// 全局指针用于信号处理
static ChannelManager* g_channelManager = nullptr;
static RemoteApiServer* g_apiServer = nullptr;

// 信号处理函数
void signalHandler(int signal) {
    qInfo() << "\n收到信号" << signal << ", 正在关闭...";
    
    if (g_apiServer) {
        g_apiServer->stop();
    }
    
    if (g_channelManager) {
        g_channelManager->stopAll();
    }
    
    QCoreApplication::quit();
}

void printBanner() {
    qInfo() << "";
    qInfo() << "╔══════════════════════════════════════════════════════════╗";
    qInfo() << "║       Modbus PlexLink - 边缘网关服务                      ║";
    qInfo() << "║       Edge Gateway Service for Industrial IoT            ║";
    qInfo() << "╠══════════════════════════════════════════════════════════╣";
    qInfo() << "║  版本: 1.1.0                                             ║";
    qInfo() << "║  模式: 无头服务 (Headless)                               ║";
    qInfo() << "╚══════════════════════════════════════════════════════════╝";
    qInfo() << "";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // 设置应用程序信息
    QCoreApplication::setOrganizationName("ModbusPlexLink");
    QCoreApplication::setOrganizationDomain("modbusplexlink.com");
    QCoreApplication::setApplicationName("Modbus PlexLink Service");
    QCoreApplication::setApplicationVersion("1.1.0");
    
    // 命令行解析
    QCommandLineParser parser;
    parser.setApplicationDescription("Modbus PlexLink 边缘网关服务");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption configOption(
        QStringList() << "c" << "config",
        "配置文件路径",
        "file",
        "config.json"
    );
    parser.addOption(configOption);
    
    QCommandLineOption httpPortOption(
        QStringList() << "p" << "http-port",
        "HTTP API 端口",
        "port",
        "8080"
    );
    parser.addOption(httpPortOption);
    
    QCommandLineOption wsPortOption(
        QStringList() << "w" << "ws-port",
        "WebSocket 端口",
        "port",
        "8081"
    );
    parser.addOption(wsPortOption);
    
    QCommandLineOption authOption(
        "auth",
        "启用认证 (格式: username:password)",
        "credentials"
    );
    parser.addOption(authOption);
    
    QCommandLineOption autoStartOption(
        QStringList() << "a" << "auto-start",
        "自动启动所有通道"
    );
    parser.addOption(autoStartOption);
    
    QCommandLineOption alarmConfigOption(
        "alarm-config",
        "报警配置文件",
        "file",
        "alarm_config.json"
    );
    parser.addOption(alarmConfigOption);
    
    parser.process(app);
    
    // 打印启动信息
    printBanner();
    
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#ifdef Q_OS_UNIX
    signal(SIGHUP, signalHandler);
#endif
    
    // 加载应用配置（为命令行参数提供默认值）
    AppSettings& appSettings = AppSettings::instance();
    
    // 获取配置（命令行参数优先，未指定则使用app_settings.json中的值）
    QString configFile = parser.value(configOption);
    
    // 如果命令行没有指定端口，使用配置文件中的值
    quint16 httpPort = parser.isSet(httpPortOption) ? 
        parser.value(httpPortOption).toUShort() : appSettings.httpPort();
    quint16 wsPort = parser.isSet(wsPortOption) ?
        parser.value(wsPortOption).toUShort() : appSettings.wsPort();
    bool autoStart = parser.isSet(autoStartOption) || appSettings.autoStartChannels();
    QString alarmConfig = parser.isSet(alarmConfigOption) ?
        parser.value(alarmConfigOption) : appSettings.alarmConfigFile();
    
    qInfo() << "[配置] 配置文件:" << configFile;
    qInfo() << "[配置] HTTP端口:" << httpPort;
    qInfo() << "[配置] WebSocket端口:" << wsPort;
    qInfo() << "[配置] 自动启动:" << (autoStart ? "是" : "否");
    qInfo() << "[配置] 报警配置:" << alarmConfig;
    qInfo() << "";
    
    // 创建通道管理器
    ChannelManager channelManager;
    g_channelManager = &channelManager;
    
    // 加载配置文件（在创建AlarmManager之前加载，这样AlarmManager能连接到所有通道）
    if (QFile::exists(configFile)) {
        qInfo() << "[启动] 加载配置文件:" << configFile;
        if (channelManager.loadConfig(configFile)) {
            qInfo() << "[成功] 配置加载完成";
            qInfo() << "  - 通道数:" << channelManager.getChannelCount();
        } else {
            qWarning() << "[警告] 配置加载失败";
        }
    } else {
        qInfo() << "[信息] 配置文件不存在，以空配置启动";
        qInfo() << "[提示] 可通过API创建通道和采集器";
    }
    
    // 创建报警管理器（会自动连接到所有已存在的通道）
    AlarmManager alarmManager(&channelManager);
    
    // 加载报警配置
    if (QFile::exists(alarmConfig)) {
        if (alarmManager.loadConfig(alarmConfig)) {
            qInfo() << "[成功] 报警配置加载完成";
        }
    }
    
    // 创建远程API服务器
    RemoteApiServer apiServer(&channelManager, &alarmManager);
    g_apiServer = &apiServer;
    
    // 设置认证（命令行参数优先，否则使用配置文件）
    if (parser.isSet(authOption)) {
        QString credentials = parser.value(authOption);
        int colonPos = credentials.indexOf(':');
        if (colonPos > 0) {
            QString username = credentials.left(colonPos);
            QString password = credentials.mid(colonPos + 1);
            apiServer.setAuthentication(true, username, password);
            qInfo() << "[安全] 已启用认证（命令行）, 用户:" << username;
        }
    } else if (appSettings.authEnabled()) {
        // 使用配置文件中的认证设置
        QString username = appSettings.authUsername();
        QString password = appSettings.authPassword();
        if (!username.isEmpty()) {
            apiServer.setAuthentication(true, username, password);
            qInfo() << "[安全] 已启用认证（配置文件）, 用户:" << username;
        }
    }
    
    // 连接信号
    QObject::connect(&apiServer, &RemoteApiServer::clientConnected, [](const QString& id) {
        qInfo() << "[连接] 客户端已连接:" << id;
    });
    
    QObject::connect(&apiServer, &RemoteApiServer::clientDisconnected, [](const QString& id) {
        qInfo() << "[断开] 客户端已断开:" << id;
    });
    
    QObject::connect(&apiServer, &RemoteApiServer::requestReceived, 
                    [](const QString& method, const QString& path) {
        qDebug() << "[请求]" << method << path;
    });
    
    QObject::connect(&apiServer, &RemoteApiServer::serverError, [](const QString& error) {
        qCritical() << "[错误]" << error;
    });
    
    // 启动API服务器
    qInfo() << "[启动] 正在启动远程API服务...";
    if (!apiServer.start(httpPort, wsPort)) {
        qCritical() << "[失败] API服务器启动失败";
        return 1;
    }
    
    qInfo() << "";
    qInfo() << "═══════════════════════════════════════════════════════════";
    qInfo() << "  服务已启动!";
    qInfo() << "";
    qInfo() << "  HTTP API:   " << apiServer.getHttpAddress();
    qInfo() << "  WebSocket:  " << apiServer.getWebSocketAddress();
    qInfo() << "";
    qInfo() << "  API文档:    " << apiServer.getHttpAddress() << "/api/system";
    qInfo() << "═══════════════════════════════════════════════════════════";
    qInfo() << "";
    
    // 自动启动通道
    if (autoStart && channelManager.getChannelCount() > 0) {
        qInfo() << "[自动] 启动所有通道...";
        QTimer::singleShot(1000, [&channelManager]() {
            channelManager.startAll();
            qInfo() << "[成功] 所有通道已启动";
        });
    }
    
    qInfo() << "[运行] 服务正在运行, 按 Ctrl+C 停止";
    qInfo() << "";
    
    // 运行事件循环
    int result = app.exec();
    
    // 清理
    qInfo() << "";
    qInfo() << "[关闭] 正在保存配置...";
    channelManager.saveConfig(configFile);
    alarmManager.saveConfig(alarmConfig);
    
    qInfo() << "[完成] 服务已停止";
    
    return result;
}
