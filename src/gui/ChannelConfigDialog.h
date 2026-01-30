#ifndef CHANNELCONFIGDIALOG_H
#define CHANNELCONFIGDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include "core/Channel.h"
#include "core/DataTypes.h"
#include "VirtualDeviceConfigDialog.h"

namespace ModbusPlexLink {
class ChannelManager;
class DataMonitorWidget;
}

namespace ModbusPlexLink {

/**
 * @brief 通道配置对话框
 * 
 * 功能：
 * - 配置通道基本信息（名称、描述）
 * - 管理采集器列表（增删改）
 * - 管理服务器列表（增删改）
 * - 配置细节通过子对话框完成
 */
class ChannelConfigDialog : public QDialog {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * @param channel 编辑的通道（nullptr表示新建）
     * @param channelManager 用于数据预览的通道管理器
     * @param parent 父窗口
     */
    explicit ChannelConfigDialog(Channel* channel = nullptr,
                                 ChannelManager* channelManager = nullptr,
                                 QWidget *parent = nullptr);
    ~ChannelConfigDialog();
    
    /**
     * @brief 获取配置
     */
    ChannelConfig getConfig() const;
    
    /**
     * @brief 设置配置
     */
    void setConfig(const ChannelConfig& config);
    
private slots:
    // 基本信息
    void onNameChanged(const QString& text);
    void onTypeChanged(int index);
    
    // 采集器操作
    void onAddCollector();
    void onEditCollector();
    void onDeleteCollector();
    void onCollectorTableSelectionChanged();
    void onCollectorTableDoubleClicked(int row, int column);
    
    // 服务器操作
    void onAddServer();
    void onEditServer();
    void onDeleteServer();
    void onServerTableSelectionChanged();
    void onServerTableDoubleClicked(int row, int column);
    
    // 对话框按钮
    void onAccepted();
    void onRejected();
    
private:
    void setupUi();
    void createBasicInfoTab();
    void createCollectorTab();
    void createServerTab();
    void createMonitorTab();
    
    void loadConfig();
    bool validateConfig();
    
    void refreshCollectorTable();
    void refreshServerTable();
    
    int getSelectedCollectorRow() const;
    int getSelectedServerRow() const;
    
    void updateCollectorButtons();
    void updateServerButtons();
    
    // 获取所有采集器变量（用于服务器配置）
    QList<AvailableVariable> getAvailableVariables() const;
    
private:
    Channel* m_channel;  // 编辑的通道（nullptr表示新建）
    ChannelManager* m_channelManager;
    ChannelConfig m_config;
    bool m_isNewChannel;
    
    // UI组件
    QTabWidget* m_tabWidget;
    
    // === Tab 1: 基本信息 ===
    QLineEdit* m_nameEdit;
    QComboBox* m_typeComboBox;      // 通道类型选择
    QCheckBox* m_enabledCheck;
    QTextEdit* m_descriptionEdit;
    
    // === Tab 2: 采集器管理 ===
    QTableWidget* m_collectorTable;
    QPushButton* m_addCollectorBtn;
    QPushButton* m_editCollectorBtn;
    QPushButton* m_deleteCollectorBtn;
    QLabel* m_collectorCountLabel;
    
    // === Tab 3: 服务器管理 ===
    QTableWidget* m_serverTable;
    QPushButton* m_addServerBtn;
    QPushButton* m_editServerBtn;
    QPushButton* m_deleteServerBtn;
    QLabel* m_serverCountLabel;
    
    // === Tab 4: 数据监控 ===
    DataMonitorWidget* m_dataMonitorWidget;
    QLabel* m_monitorInfoLabel;
    
    // 临时存储采集器和服务器配置
    QList<QJsonObject> m_collectors;
    QList<QJsonObject> m_servers;
};

} // namespace ModbusPlexLink

#endif // CHANNELCONFIGDIALOG_H
