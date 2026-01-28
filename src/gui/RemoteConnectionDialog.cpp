#include "RemoteConnectionDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace ModbusPlexLink {

RemoteConnectionDialog::RemoteConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("连接到远程服务"));
    setMinimumWidth(400);
    setModal(true);
    
    setupUi();
    loadSettings();
    updateConnectButton();
}

RemoteConnectionDialog::~RemoteConnectionDialog() {
    if (m_rememberCheck->isChecked()) {
        saveSettings();
    }
}

void RemoteConnectionDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    
    // 当前连接状态提示（初始隐藏）
    m_currentConnectionLabel = new QLabel();
    m_currentConnectionLabel->setWordWrap(true);
    m_currentConnectionLabel->hide();
    mainLayout->addWidget(m_currentConnectionLabel);
    
    // 服务器信息组
    QGroupBox* serverGroup = new QGroupBox(tr("服务器信息"));
    QFormLayout* serverLayout = new QFormLayout(serverGroup);
    
    m_hostEdit = new QLineEdit();
    m_hostEdit->setPlaceholderText(tr("例如: 192.168.1.100 或 localhost"));
    m_hostEdit->setText("localhost");
    connect(m_hostEdit, &QLineEdit::textChanged, this, &RemoteConnectionDialog::updateConnectButton);
    serverLayout->addRow(tr("主机地址:"), m_hostEdit);
    
    QHBoxLayout* portLayout = new QHBoxLayout();
    m_httpPortSpin = new QSpinBox();
    m_httpPortSpin->setRange(1, 65535);
    m_httpPortSpin->setValue(8080);
    portLayout->addWidget(new QLabel(tr("HTTP:")));
    portLayout->addWidget(m_httpPortSpin);
    
    m_wsPortSpin = new QSpinBox();
    m_wsPortSpin->setRange(1, 65535);
    m_wsPortSpin->setValue(8081);
    portLayout->addWidget(new QLabel(tr("WebSocket:")));
    portLayout->addWidget(m_wsPortSpin);
    portLayout->addStretch();
    serverLayout->addRow(tr("端口:"), portLayout);
    
    mainLayout->addWidget(serverGroup);
    
    // 认证组
    QGroupBox* authGroup = new QGroupBox(tr("认证设置"));
    QVBoxLayout* authLayout = new QVBoxLayout(authGroup);
    
    m_authCheck = new QCheckBox(tr("需要认证"));
    connect(m_authCheck, &QCheckBox::toggled, this, &RemoteConnectionDialog::onAuthCheckChanged);
    authLayout->addWidget(m_authCheck);
    
    QFormLayout* credLayout = new QFormLayout();
    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText(tr("用户名"));
    m_usernameEdit->setEnabled(false);
    credLayout->addRow(tr("用户名:"), m_usernameEdit);
    
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setPlaceholderText(tr("密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setEnabled(false);
    credLayout->addRow(tr("密码:"), m_passwordEdit);
    
    authLayout->addLayout(credLayout);
    mainLayout->addWidget(authGroup);
    
    // 选项
    QGroupBox* optionsGroup = new QGroupBox(tr("选项"));
    QVBoxLayout* optLayout = new QVBoxLayout(optionsGroup);
    
    m_autoReconnectCheck = new QCheckBox(tr("断线自动重连"));
    m_autoReconnectCheck->setChecked(true);
    optLayout->addWidget(m_autoReconnectCheck);
    
    m_rememberCheck = new QCheckBox(tr("记住连接设置"));
    m_rememberCheck->setChecked(true);
    optLayout->addWidget(m_rememberCheck);
    
    mainLayout->addWidget(optionsGroup);
    
    // 状态标签
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: #6B7280; font-style: italic;");
    mainLayout->addWidget(m_statusLabel);
    
    // 按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    m_testBtn = new QPushButton(tr("测试连接"));
    connect(m_testBtn, &QPushButton::clicked, this, &RemoteConnectionDialog::onTestClicked);
    btnLayout->addWidget(m_testBtn);
    
    m_cancelBtn = new QPushButton(tr("取消"));
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);
    
    m_connectBtn = new QPushButton(tr("连接"));
    m_connectBtn->setDefault(true);
    m_connectBtn->setStyleSheet(
        "QPushButton { background-color: #3B82F6; color: white; padding: 8px 24px; "
        "border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2563EB; }"
        "QPushButton:disabled { background-color: #9CA3AF; }"
    );
    connect(m_connectBtn, &QPushButton::clicked, this, &RemoteConnectionDialog::onConnectClicked);
    btnLayout->addWidget(m_connectBtn);
    
    mainLayout->addLayout(btnLayout);
}

void RemoteConnectionDialog::loadSettings() {
    QSettings settings("ModbusPlexLink", "RemoteConnection");
    m_hostEdit->setText(settings.value("host", "localhost").toString());
    m_httpPortSpin->setValue(settings.value("httpPort", 8080).toInt());
    m_wsPortSpin->setValue(settings.value("wsPort", 8081).toInt());
    m_authCheck->setChecked(settings.value("useAuth", false).toBool());
    m_usernameEdit->setText(settings.value("username", "").toString());
    m_autoReconnectCheck->setChecked(settings.value("autoReconnect", true).toBool());
    m_rememberCheck->setChecked(settings.value("remember", true).toBool());
}

void RemoteConnectionDialog::saveSettings() {
    QSettings settings("ModbusPlexLink", "RemoteConnection");
    settings.setValue("host", m_hostEdit->text());
    settings.setValue("httpPort", m_httpPortSpin->value());
    settings.setValue("wsPort", m_wsPortSpin->value());
    settings.setValue("useAuth", m_authCheck->isChecked());
    settings.setValue("username", m_usernameEdit->text());
    settings.setValue("autoReconnect", m_autoReconnectCheck->isChecked());
    settings.setValue("remember", m_rememberCheck->isChecked());
    // 不保存密码
}

QString RemoteConnectionDialog::getHost() const {
    return m_hostEdit->text().trimmed();
}

quint16 RemoteConnectionDialog::getHttpPort() const {
    return static_cast<quint16>(m_httpPortSpin->value());
}

quint16 RemoteConnectionDialog::getWsPort() const {
    return static_cast<quint16>(m_wsPortSpin->value());
}

QString RemoteConnectionDialog::getUsername() const {
    return m_usernameEdit->text();
}

QString RemoteConnectionDialog::getPassword() const {
    return m_passwordEdit->text();
}

bool RemoteConnectionDialog::useAuthentication() const {
    return m_authCheck->isChecked();
}

bool RemoteConnectionDialog::autoReconnect() const {
    return m_autoReconnectCheck->isChecked();
}

void RemoteConnectionDialog::setHost(const QString& host) {
    m_hostEdit->setText(host);
}

void RemoteConnectionDialog::setHttpPort(quint16 port) {
    m_httpPortSpin->setValue(port);
}

void RemoteConnectionDialog::setWsPort(quint16 port) {
    m_wsPortSpin->setValue(port);
}

void RemoteConnectionDialog::setCredentials(const QString& username, const QString& password) {
    m_authCheck->setChecked(true);
    m_usernameEdit->setText(username);
    m_passwordEdit->setText(password);
}

void RemoteConnectionDialog::setCurrentConnection(const QString& currentHost, bool isConnected) {
    if (currentHost.isEmpty()) {
        m_currentConnectionLabel->hide();
        return;
    }
    
    if (isConnected) {
        m_currentConnectionLabel->setText(
            tr("⚠️ <b>当前已连接到: %1</b><br>"
               "<span style='color: #6B7280;'>连接新的远程服务将断开当前连接</span>")
            .arg(currentHost));
        m_currentConnectionLabel->setStyleSheet(
            "QLabel { background-color: #FEF3C7; border: 1px solid #F59E0B; "
            "border-radius: 6px; padding: 12px; color: #92400E; }");
        m_connectBtn->setText(tr("切换连接"));
    } else {
        m_currentConnectionLabel->setText(
            tr("ℹ️ <b>上次连接: %1</b> (已断开)<br>"
               "<span style='color: #6B7280;'>您可以重新连接此服务或连接其他服务</span>")
            .arg(currentHost));
        m_currentConnectionLabel->setStyleSheet(
            "QLabel { background-color: #DBEAFE; border: 1px solid #3B82F6; "
            "border-radius: 6px; padding: 12px; color: #1E40AF; }");
        m_connectBtn->setText(tr("连接"));
    }
    
    m_currentConnectionLabel->show();
}

void RemoteConnectionDialog::onConnectClicked() {
    if (m_rememberCheck->isChecked()) {
        saveSettings();
    }
    accept();
}

void RemoteConnectionDialog::onTestClicked() {
    m_statusLabel->setText(tr("正在测试连接..."));
    m_statusLabel->setStyleSheet("color: #F59E0B;");
    m_testBtn->setEnabled(false);
    
    // 测试HTTP连接
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    
    QUrl url;
    url.setScheme("http");
    url.setHost(getHost());
    url.setPort(getHttpPort());
    url.setPath("/api/system");
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    if (useAuthentication()) {
        QString credentials = QString("%1:%2").arg(getUsername()).arg(getPassword());
        QByteArray encoded = credentials.toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + encoded);
    }
    
    QNetworkReply* reply = manager->get(request);
    
    // 设置超时
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [reply, this]() {
        reply->abort();
        m_statusLabel->setText(tr("连接超时"));
        m_statusLabel->setStyleSheet("color: #EF4444;");
        m_testBtn->setEnabled(true);
    });
    timer->start(5000);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, manager, timer]() {
        timer->stop();
        timer->deleteLater();
        m_testBtn->setEnabled(true);
        
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj["success"].toBool()) {
                    QJsonObject sys = obj["system"].toObject();
                    QString info = QString("%1 v%2 - %3 通道")
                        .arg(sys["name"].toString())
                        .arg(sys["version"].toString())
                        .arg(sys["channelCount"].toInt());
                    m_statusLabel->setText(tr("连接成功: %1").arg(info));
                    m_statusLabel->setStyleSheet("color: #10B981;");
                } else {
                    m_statusLabel->setText(tr("服务器响应异常"));
                    m_statusLabel->setStyleSheet("color: #EF4444;");
                }
            }
        } else if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
            m_statusLabel->setText(tr("认证失败，请检查用户名和密码"));
            m_statusLabel->setStyleSheet("color: #EF4444;");
        } else {
            m_statusLabel->setText(tr("连接失败: %1").arg(reply->errorString()));
            m_statusLabel->setStyleSheet("color: #EF4444;");
        }
        
        reply->deleteLater();
        manager->deleteLater();
    });
}

void RemoteConnectionDialog::onAuthCheckChanged(bool checked) {
    m_usernameEdit->setEnabled(checked);
    m_passwordEdit->setEnabled(checked);
}

void RemoteConnectionDialog::updateConnectButton() {
    m_connectBtn->setEnabled(!m_hostEdit->text().trimmed().isEmpty());
}

} // namespace ModbusPlexLink
