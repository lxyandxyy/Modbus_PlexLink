#ifndef REMOTECONNECTIONDIALOG_H
#define REMOTECONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QSettings>

namespace ModbusPlexLink {

/**
 * @brief 远程连接对话框
 * 
 * 允许用户输入远程服务器地址并连接
 */
class RemoteConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit RemoteConnectionDialog(QWidget *parent = nullptr);
    ~RemoteConnectionDialog();

    // 获取连接参数
    QString getHost() const;
    quint16 getHttpPort() const;
    quint16 getWsPort() const;
    QString getUsername() const;
    QString getPassword() const;
    bool useAuthentication() const;
    bool autoReconnect() const;

    // 设置连接参数（用于编辑现有连接）
    void setHost(const QString& host);
    void setHttpPort(quint16 port);
    void setWsPort(quint16 port);
    void setCredentials(const QString& username, const QString& password);
    
    // 设置当前连接状态（用于显示提示）
    void setCurrentConnection(const QString& currentHost, bool isConnected);

private slots:
    void onConnectClicked();
    void onTestClicked();
    void onAuthCheckChanged(bool checked);
    void updateConnectButton();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    // UI组件
    QLineEdit* m_hostEdit;
    QSpinBox* m_httpPortSpin;
    QSpinBox* m_wsPortSpin;
    QCheckBox* m_authCheck;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QCheckBox* m_autoReconnectCheck;
    QCheckBox* m_rememberCheck;
    
    QPushButton* m_connectBtn;
    QPushButton* m_testBtn;
    QPushButton* m_cancelBtn;
    
    QLabel* m_statusLabel;
    QLabel* m_currentConnectionLabel;  // 显示当前连接状态
};

} // namespace ModbusPlexLink

#endif // REMOTECONNECTIONDIALOG_H
