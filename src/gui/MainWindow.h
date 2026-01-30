#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QLabel>
#include <QTimer>
#include <QAction>
#include <QToolBar>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QSplitter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFrame>
#include "core/ChannelManager.h"
#include "utils/AlarmManager.h"
#include "remote/RemoteClient.h"
#include "remote/RemoteApiServer.h"

// 前向声明（全局命名空间）
class WaveformRecorderWidget;

namespace ModbusPlexLink {

class LogViewerWidget;
class AlarmWidget;
class ChannelCardWidget;
class DashboardDataWidget;
class MessageLogPanel;
class RemoteConnectionDialog;

/**
 * @brief 应用程序运行模式
 */
enum class ApplicationMode {
    LocalWithApi,    // 本地采集 + 远程API服务（默认，完整功能）
    LocalOnly,       // 纯本地采集（不启动远程API）
    RemoteClient     // 纯远程客户端模式（连接其他网关）
};

/**
 * @brief 醒目的模式状态指示器组件
 */
class ModeStatusIndicator : public QFrame {
    Q_OBJECT
public:
    explicit ModeStatusIndicator(QWidget* parent = nullptr);
    
    void setLocalMode(bool withApi, quint16 httpPort = 0, quint16 wsPort = 0);
    void setRemoteMode(const QString& remoteHost, bool connected);
    void setDisconnected();
    
signals:
    void clicked();
    
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    void updateDisplay();
    
    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_detailLabel;
    
    ApplicationMode m_mode;
    bool m_isConnected;
    QString m_remoteHost;
    quint16 m_httpPort;
    quint16 m_wsPort;
};

/**
 * @brief 主窗口 - 通道管理界面
 * 
 * 功能：
 * - 显示所有通道的列表
 * - 通道的启动、停止、编辑、删除操作
 * - 配置文件的加载和保存
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
private slots:
    // 文件菜单
    void onNewChannel();
    void onBatchCreateChannels();
    void onLoadConfig();
    void onSaveConfig();
    void onSaveConfigAs();
    void onExit();
    
    // 通道操作
    void onEditChannel();
    void onDeleteChannel();
    void onStartChannel();
    void onStopChannel();
    void onStartAllChannels();
    void onStopAllChannels();
    
    // 帮助菜单
    void onAbout();
    void onShowAppSettings();  // 应用设置

    // 工具
    void onShowLogViewer();
    void onShowAlarmManager();
    void onShowWaveformRecorder();
    void onShowSystemVariables();
    void onAlarmTriggered(const AlarmEvent& event);
    void onPlaybackAlarmRecording(const QString& csvFilePath, const AlarmRecordingData& recordingData);
    
    // 远程连接
    void onConnectRemote();
    void onDisconnectRemote();
    void onSwitchToLocalMode();  // 新增：切换回本地模式
    void onRemoteConnected();
    void onRemoteDisconnected();
    void onRemoteError(const QString& error);
    void onRemoteChannelsReceived(const QJsonArray& channels);
    void onRemoteDataReceived(const QString& channelName, const QJsonObject& data);

    // 通道管理器信号
    void onChannelCreated(const QString& name);
    void onChannelDeleted(const QString& name);
    void onChannelStateChanged(const QString& name, ChannelState state);
    
    // 界面更新
    // void onChannelTableSelectionChanged();  // 已弃用：使用卡片代替表格
    // void onChannelTableDoubleClicked(int row, int column);  // 已弃用
    void updateChannelTable();
    void updateStatusBar();
    
    // 系统托盘
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowHide();
    void onTrayExit();
    
protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    
private:
    void setupUi();
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void createCentralWidget();
    void createSystemTray();
    void applyModernTheme();  // 新增：应用现代化主题

    QString getSelectedChannelName() const;
    Channel* getSelectedChannel() const;  // 新增：获取选中的通道对象
    void selectChannel(Channel* channel);  // 新增：选中通道
    void updateChannelCards();  // 新增：更新通道卡片列表
    void updateActions();
    bool saveConfigIfNeeded();
    void saveConfigWithChannelRestart(const QString& filename);
    
    QString channelStateToString(ChannelState state) const;
    QColor channelStateColor(ChannelState state) const;
    QString getChannelConnectionStatus(Channel* channel) const;
    
    void showTrayMessage(const QString& title, const QString& message);
    void autoStartAllChannels();
    void updateAlarmIndicator();
    
private:
    // 核心对象
    ChannelManager* m_channelManager;
    AlarmManager* m_alarmManager;

    // 新的UI组件
    QSplitter* m_mainSplitter;           // 主分割器（左右）
    QSplitter* m_rightSplitter;          // 右侧分割器（上下）

    // 左侧：通道列表
    QScrollArea* m_channelScrollArea;
    QWidget* m_channelListWidget;
    QVBoxLayout* m_channelListLayout;
    QList<ChannelCardWidget*> m_channelCards;

    // 右上：仪表盘数据
    DashboardDataWidget* m_dashboardWidget;

    // 右下：报文与日志
    MessageLogPanel* m_messageLogPanel;

    // 当前选中的通道
    Channel* m_currentChannel;

    // 旧的UI组件（仅作为独立工具窗口）
    QTableWidget* m_channelTable;  // 已废弃，用卡片替代
    QTimer* m_updateTimer;
    
    // 状态栏
    QLabel* m_statusLabel;
    QLabel* m_channelCountLabel;
    QLabel* m_runningCountLabel;
    QLabel* m_alarmIndicator;
    
    // 菜单和工具栏动作
    QAction* m_newChannelAction;
    QAction* m_batchCreateAction;
    QAction* m_loadConfigAction;
    QAction* m_saveConfigAction;
    QAction* m_saveConfigAsAction;
    QAction* m_exitAction;
    
    QAction* m_editChannelAction;
    QAction* m_deleteChannelAction;
    QAction* m_startChannelAction;
    QAction* m_stopChannelAction;
    QAction* m_startAllAction;
    QAction* m_stopAllAction;
    
    QAction* m_aboutAction;
    QAction* m_logViewerAction;
    QAction* m_alarmManagerAction;
    QAction* m_waveformRecorderAction;
    QAction* m_systemVariableAction;
    
    // 远程连接动作
    QAction* m_connectRemoteAction;
    QAction* m_disconnectRemoteAction;
    QAction* m_switchToLocalAction;  // 新增：切换到本地模式
    
    // 应用设置
    QAction* m_appSettingsAction;    // 应用设置对话框
    
    // 配置文件
    QString m_currentConfigFile;
    bool m_configModified;
    
    // 系统托盘
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    QAction* m_showHideAction;
    QAction* m_trayExitAction;
    
    // 配置选项
    bool m_autoStartChannels;

    // 工具窗口
    LogViewerWidget* m_logViewer;
    AlarmWidget* m_alarmWidget;
    ::WaveformRecorderWidget* m_waveformRecorder;  // 使用全局命名空间
    
    // 远程连接
    RemoteClient* m_remoteClient;
    bool m_isRemoteMode;
    QString m_remoteHost;
    QLabel* m_connectionStatusLabel;
    
    // 本地API服务器（方案一：GUI模式也可被远程访问）
    RemoteApiServer* m_localApiServer;
    ApplicationMode m_appMode;
    quint16 m_localHttpPort;
    quint16 m_localWsPort;
    
    // 醒目的模式状态指示器
    ModeStatusIndicator* m_modeIndicator;
    
    // 远程通道数据缓存（远程模式下使用）
    QJsonArray m_remoteChannelsCache;
    QMap<QString, QJsonObject> m_remoteDataCache;  // channelName -> data
    QString m_pendingEditRemoteChannel;  // 待编辑的远程通道名
    
private:
    // 模式管理
    void initializeMode(ApplicationMode mode);
    void startLocalApiServer();
    void stopLocalApiServer();
    void enterRemoteMode();
    void exitRemoteMode();
    
    // 远程通道卡片管理
    void updateRemoteChannelCards();
    void clearChannelCards();
};

} // namespace ModbusPlexLink

#endif // MAINWINDOW_H
