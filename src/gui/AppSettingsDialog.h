#ifndef APPSETTINGSDIALOG_H
#define APPSETTINGSDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QTabWidget>

namespace ModbusPlexLink {

/**
 * @brief 应用设置对话框
 * 
 * 允许用户配置：
 * - API服务器端口
 * - 认证设置
 * - 启动行为
 */
class AppSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit AppSettingsDialog(QWidget* parent = nullptr);
    ~AppSettingsDialog() = default;

private slots:
    void onAccepted();
    void onApiEnabledChanged(bool enabled);
    void onAuthEnabledChanged(bool enabled);
    void onResetDefaults();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    
    // API设置
    QCheckBox* m_apiEnabledCheck;
    QSpinBox* m_httpPortSpin;
    QSpinBox* m_wsPortSpin;
    
    // 认证设置
    QCheckBox* m_authEnabledCheck;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    
    // 启动设置
    QCheckBox* m_autoStartCheck;
    QCheckBox* m_minimizeToTrayCheck;
    
    // 文件路径
    QLineEdit* m_configFileEdit;
    QLineEdit* m_alarmConfigEdit;
    
    QTabWidget* m_tabWidget;
};

} // namespace ModbusPlexLink

#endif // APPSETTINGSDIALOG_H
