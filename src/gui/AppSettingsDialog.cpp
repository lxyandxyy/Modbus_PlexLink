#include "AppSettingsDialog.h"
#include "DialogStyles.h"
#include "utils/AppSettings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QFrame>
#include <QFileDialog>

namespace ModbusPlexLink {

AppSettingsDialog::AppSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("⚙️ 应用设置"));
    resize(550, 480);
    setMinimumSize(480, 400);
    
    setStyleSheet(DialogStyles::getDialogStyle());
    
    setupUi();
    loadSettings();
}

void AppSettingsDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);
    
    // 标题
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* iconLabel = new QLabel("⚙️", this);
    iconLabel->setStyleSheet("font-size: 24px;");
    headerLayout->addWidget(iconLabel);
    
    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    QLabel* titleLabel = new QLabel(tr("应用程序设置"), this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1E293B;");
    titleLayout->addWidget(titleLabel);
    QLabel* subtitleLabel = new QLabel(tr("配置API端口、认证和启动行为"), this);
    subtitleLabel->setStyleSheet("font-size: 10px; color: #64748B;");
    titleLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);
    
    // Tab页
    m_tabWidget = new QTabWidget(this);
    
    // === API设置页 ===
    QWidget* apiPage = new QWidget();
    apiPage->setStyleSheet("background: transparent;");
    QVBoxLayout* apiLayout = new QVBoxLayout(apiPage);
    apiLayout->setContentsMargins(16, 16, 16, 16);
    apiLayout->setSpacing(16);
    
    // API启用
    QWidget* apiCard = new QWidget(apiPage);
    apiCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 10px;
        }
    )");
    QVBoxLayout* apiCardLayout = new QVBoxLayout(apiCard);
    apiCardLayout->setContentsMargins(16, 12, 16, 12);
    apiCardLayout->setSpacing(12);
    
    m_apiEnabledCheck = new QCheckBox(tr("启用本地API服务器（允许远程管理此设备）"), apiCard);
    m_apiEnabledCheck->setStyleSheet("font-weight: 600; color: #1E293B; border: none; background: transparent;");
    connect(m_apiEnabledCheck, &QCheckBox::toggled, this, &AppSettingsDialog::onApiEnabledChanged);
    apiCardLayout->addWidget(m_apiEnabledCheck);
    
    QFrame* sep1 = new QFrame(apiCard);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("background-color: #E2E8F0; border: none;");
    sep1->setFixedHeight(1);
    apiCardLayout->addWidget(sep1);
    
    QFormLayout* portForm = new QFormLayout();
    portForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    portForm->setHorizontalSpacing(16);
    portForm->setVerticalSpacing(10);
    
    m_httpPortSpin = new QSpinBox(apiCard);
    m_httpPortSpin->setRange(1024, 65535);
    m_httpPortSpin->setValue(8080);
    m_httpPortSpin->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: #F8FAFC;");
    QLabel* httpLabel = new QLabel(tr("HTTP API 端口"), apiCard);
    httpLabel->setStyleSheet("font-weight: 500; color: #374151; border: none; background: transparent;");
    portForm->addRow(httpLabel, m_httpPortSpin);
    
    m_wsPortSpin = new QSpinBox(apiCard);
    m_wsPortSpin->setRange(1024, 65535);
    m_wsPortSpin->setValue(8081);
    m_wsPortSpin->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: #F8FAFC;");
    QLabel* wsLabel = new QLabel(tr("WebSocket 端口"), apiCard);
    wsLabel->setStyleSheet("font-weight: 500; color: #374151; border: none; background: transparent;");
    portForm->addRow(wsLabel, m_wsPortSpin);
    
    apiCardLayout->addLayout(portForm);
    
    // 提示
    QLabel* portHint = new QLabel(tr("💡 提示：如果端口被占用，请更换其他端口（如 8082/8083）"), apiCard);
    portHint->setWordWrap(true);
    portHint->setStyleSheet("color: #64748B; font-size: 9pt; border: none; background: transparent;");
    apiCardLayout->addWidget(portHint);
    
    apiLayout->addWidget(apiCard);
    apiLayout->addStretch();
    
    m_tabWidget->addTab(apiPage, tr("🌐 API服务"));
    
    // === 认证设置页 ===
    QWidget* authPage = new QWidget();
    authPage->setStyleSheet("background: transparent;");
    QVBoxLayout* authLayout = new QVBoxLayout(authPage);
    authLayout->setContentsMargins(16, 16, 16, 16);
    authLayout->setSpacing(16);
    
    QWidget* authCard = new QWidget(authPage);
    authCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 10px;
        }
    )");
    QVBoxLayout* authCardLayout = new QVBoxLayout(authCard);
    authCardLayout->setContentsMargins(16, 12, 16, 12);
    authCardLayout->setSpacing(12);
    
    m_authEnabledCheck = new QCheckBox(tr("启用认证（远程连接需要用户名和密码）"), authCard);
    m_authEnabledCheck->setStyleSheet("font-weight: 600; color: #1E293B; border: none; background: transparent;");
    connect(m_authEnabledCheck, &QCheckBox::toggled, this, &AppSettingsDialog::onAuthEnabledChanged);
    authCardLayout->addWidget(m_authEnabledCheck);
    
    QFrame* sep2 = new QFrame(authCard);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("background-color: #E2E8F0; border: none;");
    sep2->setFixedHeight(1);
    authCardLayout->addWidget(sep2);
    
    QFormLayout* authForm = new QFormLayout();
    authForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    authForm->setHorizontalSpacing(16);
    authForm->setVerticalSpacing(10);
    
    m_usernameEdit = new QLineEdit(authCard);
    m_usernameEdit->setPlaceholderText(tr("输入用户名"));
    m_usernameEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 8px 12px; background: #F8FAFC;");
    QLabel* userLabel = new QLabel(tr("用户名"), authCard);
    userLabel->setStyleSheet("font-weight: 500; color: #374151; border: none; background: transparent;");
    authForm->addRow(userLabel, m_usernameEdit);
    
    m_passwordEdit = new QLineEdit(authCard);
    m_passwordEdit->setPlaceholderText(tr("输入密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 8px 12px; background: #F8FAFC;");
    QLabel* passLabel = new QLabel(tr("密码"), authCard);
    passLabel->setStyleSheet("font-weight: 500; color: #374151; border: none; background: transparent;");
    authForm->addRow(passLabel, m_passwordEdit);
    
    authCardLayout->addLayout(authForm);
    authLayout->addWidget(authCard);
    authLayout->addStretch();
    
    m_tabWidget->addTab(authPage, tr("🔐 认证"));
    
    // === 启动设置页 ===
    QWidget* startupPage = new QWidget();
    startupPage->setStyleSheet("background: transparent;");
    QVBoxLayout* startupLayout = new QVBoxLayout(startupPage);
    startupLayout->setContentsMargins(16, 16, 16, 16);
    startupLayout->setSpacing(16);
    
    QWidget* startupCard = new QWidget(startupPage);
    startupCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 10px;
        }
    )");
    QVBoxLayout* startupCardLayout = new QVBoxLayout(startupCard);
    startupCardLayout->setContentsMargins(16, 12, 16, 12);
    startupCardLayout->setSpacing(10);
    
    m_autoStartCheck = new QCheckBox(tr("启动时自动运行所有通道"), startupCard);
    m_autoStartCheck->setStyleSheet("color: #1E293B; border: none; background: transparent;");
    startupCardLayout->addWidget(m_autoStartCheck);
    
    m_minimizeToTrayCheck = new QCheckBox(tr("关闭窗口时最小化到系统托盘"), startupCard);
    m_minimizeToTrayCheck->setStyleSheet("color: #1E293B; border: none; background: transparent;");
    startupCardLayout->addWidget(m_minimizeToTrayCheck);
    
    startupLayout->addWidget(startupCard);
    
    // 配置文件路径
    QWidget* fileCard = new QWidget(startupPage);
    fileCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 10px;
        }
    )");
    QVBoxLayout* fileCardLayout = new QVBoxLayout(fileCard);
    fileCardLayout->setContentsMargins(16, 12, 16, 12);
    fileCardLayout->setSpacing(10);
    
    QLabel* fileTitle = new QLabel(tr("📂 默认配置文件"), fileCard);
    fileTitle->setStyleSheet("font-weight: 600; color: #1E293B; border: none; background: transparent;");
    fileCardLayout->addWidget(fileTitle);
    
    QFormLayout* fileForm = new QFormLayout();
    fileForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fileForm->setHorizontalSpacing(12);
    fileForm->setVerticalSpacing(8);
    
    m_configFileEdit = new QLineEdit(fileCard);
    m_configFileEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: #F8FAFC;");
    QLabel* cfgLabel = new QLabel(tr("通道配置"), fileCard);
    cfgLabel->setStyleSheet("font-weight: 500; color: #374151; border: none; background: transparent;");
    fileForm->addRow(cfgLabel, m_configFileEdit);
    
    m_alarmConfigEdit = new QLineEdit(fileCard);
    m_alarmConfigEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: #F8FAFC;");
    QLabel* alarmLabel = new QLabel(tr("报警配置"), fileCard);
    alarmLabel->setStyleSheet("font-weight: 500; color: #374151; border: none; background: transparent;");
    fileForm->addRow(alarmLabel, m_alarmConfigEdit);
    
    fileCardLayout->addLayout(fileForm);
    startupLayout->addWidget(fileCard);
    startupLayout->addStretch();
    
    m_tabWidget->addTab(startupPage, tr("🚀 启动"));
    
    mainLayout->addWidget(m_tabWidget, 1);
    
    // 分隔线
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: #E2E8F0;");
    line->setFixedHeight(1);
    mainLayout->addWidget(line);
    
    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    QPushButton* resetBtn = DialogStyles::createSecondaryButton(tr("恢复默认"), this);
    connect(resetBtn, &QPushButton::clicked, this, &AppSettingsDialog::onResetDefaults);
    
    QPushButton* cancelBtn = DialogStyles::createSecondaryButton(tr("取消"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    QPushButton* saveBtn = DialogStyles::createPrimaryButton(tr("保存设置"), this);
    connect(saveBtn, &QPushButton::clicked, this, &AppSettingsDialog::onAccepted);
    
    buttonLayout->addWidget(resetBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(saveBtn);
    
    mainLayout->addLayout(buttonLayout);
}

void AppSettingsDialog::loadSettings() {
    AppSettings& settings = AppSettings::instance();
    
    m_apiEnabledCheck->setChecked(settings.apiEnabled());
    m_httpPortSpin->setValue(settings.httpPort());
    m_wsPortSpin->setValue(settings.wsPort());
    
    m_authEnabledCheck->setChecked(settings.authEnabled());
    m_usernameEdit->setText(settings.authUsername());
    m_passwordEdit->setText(settings.authPassword());
    
    m_autoStartCheck->setChecked(settings.autoStartChannels());
    m_minimizeToTrayCheck->setChecked(settings.minimizeToTray());
    
    m_configFileEdit->setText(settings.defaultConfigFile());
    m_alarmConfigEdit->setText(settings.alarmConfigFile());
    
    // 更新控件状态
    onApiEnabledChanged(m_apiEnabledCheck->isChecked());
    onAuthEnabledChanged(m_authEnabledCheck->isChecked());
}

void AppSettingsDialog::saveSettings() {
    AppSettings& settings = AppSettings::instance();
    
    settings.setApiEnabled(m_apiEnabledCheck->isChecked());
    settings.setHttpPort(static_cast<quint16>(m_httpPortSpin->value()));
    settings.setWsPort(static_cast<quint16>(m_wsPortSpin->value()));
    
    settings.setAuthEnabled(m_authEnabledCheck->isChecked());
    settings.setAuthUsername(m_usernameEdit->text());
    settings.setAuthPassword(m_passwordEdit->text());
    
    settings.setAutoStartChannels(m_autoStartCheck->isChecked());
    settings.setMinimizeToTray(m_minimizeToTrayCheck->isChecked());
    
    settings.setDefaultConfigFile(m_configFileEdit->text());
    settings.setAlarmConfigFile(m_alarmConfigEdit->text());
    
    settings.save();
}

void AppSettingsDialog::onAccepted() {
    saveSettings();
    
    QMessageBox::information(this, tr("设置已保存"),
        tr("应用设置已保存。\n\n"
           "注意：端口更改需要重启应用程序才能生效。"));
    
    accept();
}

void AppSettingsDialog::onApiEnabledChanged(bool enabled) {
    m_httpPortSpin->setEnabled(enabled);
    m_wsPortSpin->setEnabled(enabled);
}

void AppSettingsDialog::onAuthEnabledChanged(bool enabled) {
    m_usernameEdit->setEnabled(enabled);
    m_passwordEdit->setEnabled(enabled);
}

void AppSettingsDialog::onResetDefaults() {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("恢复默认设置"),
        tr("确定要将所有设置恢复为默认值吗？"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        AppSettings::instance().resetToDefaults();
        loadSettings();
    }
}

} // namespace ModbusPlexLink
