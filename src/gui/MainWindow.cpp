#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QThread>
#include <QToolBar>
#include <QVBoxLayout>

#include "AlarmWidget.h"
#include "AppSettingsDialog.h"
#include "ChannelCardWidget.h"
#include "ChannelConfigDialog.h"
#include "DashboardDataWidget.h"
#include "LogViewerWidget.h"
#include "MessageLogPanel.h"
#include "RemoteConnectionDialog.h"
#include "WaveformRecorder.h"
#include "utils/AppSettings.h"

namespace ModbusPlexLink {

// ============================================================================
// ModeStatusIndicator 实现 - 醒目的模式状态指示器
// ============================================================================

ModeStatusIndicator::ModeStatusIndicator(QWidget* parent)
    : QFrame(parent),
      m_mode(ApplicationMode::LocalWithApi),
      m_isConnected(false),
      m_httpPort(8080),
      m_wsPort(8081) {
  setObjectName("modeStatusIndicator");
  setCursor(Qt::PointingHandCursor);
  setMinimumHeight(60);
  setMaximumHeight(70);

  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(16, 10, 16, 10);
  layout->setSpacing(12);

  // 图标
  m_iconLabel = new QLabel(this);
  m_iconLabel->setFixedSize(40, 40);
  m_iconLabel->setAlignment(Qt::AlignCenter);
  QFont iconFont = m_iconLabel->font();
  iconFont.setPointSize(20);
  m_iconLabel->setFont(iconFont);
  layout->addWidget(m_iconLabel);

  // 文字区域
  QVBoxLayout* textLayout = new QVBoxLayout();
  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->setSpacing(2);

  m_titleLabel = new QLabel(this);
  QFont titleFont = m_titleLabel->font();
  titleFont.setPointSize(11);
  titleFont.setBold(true);
  m_titleLabel->setFont(titleFont);
  textLayout->addWidget(m_titleLabel);

  m_detailLabel = new QLabel(this);
  QFont detailFont = m_detailLabel->font();
  detailFont.setPointSize(9);
  m_detailLabel->setFont(detailFont);
  textLayout->addWidget(m_detailLabel);

  layout->addLayout(textLayout);
  layout->addStretch();

  // 默认显示
  setLocalMode(true, 8080, 8081);
}

void ModeStatusIndicator::setLocalMode(bool withApi, quint16 httpPort,
                                       quint16 wsPort) {
  m_mode = withApi ? ApplicationMode::LocalWithApi : ApplicationMode::LocalOnly;
  m_httpPort = httpPort;
  m_wsPort = wsPort;
  m_isConnected = true;
  m_remoteHost.clear();
  updateDisplay();
}

void ModeStatusIndicator::setRemoteMode(const QString& remoteHost,
                                        bool connected) {
  m_mode = ApplicationMode::RemoteClient;
  m_remoteHost = remoteHost;
  m_isConnected = connected;
  updateDisplay();
}

void ModeStatusIndicator::setDisconnected() {
  m_isConnected = false;
  updateDisplay();
}

void ModeStatusIndicator::updateDisplay() {
  QString bgColor, borderColor, textColor, iconBgColor;
  QString icon, title, detail;

  switch (m_mode) {
    case ApplicationMode::LocalWithApi:
      icon = "🖥️";
      title = tr("本地模式 + 远程API");
      detail = tr("HTTP: %1 | WebSocket: %2").arg(m_httpPort).arg(m_wsPort);
      bgColor = "#ECFDF5";
      borderColor = "#10B981";
      textColor = "#065F46";
      iconBgColor = "#D1FAE5";
      break;

    case ApplicationMode::LocalOnly:
      icon = "💻";
      title = tr("本地模式");
      detail = tr("仅本地采集，未启用远程API");
      bgColor = "#F0F9FF";
      borderColor = "#3B82F6";
      textColor = "#1E40AF";
      iconBgColor = "#DBEAFE";
      break;

    case ApplicationMode::RemoteClient:
      if (m_isConnected) {
        icon = "🌐";
        title = tr("远程模式 - 已连接");
        detail = m_remoteHost;
        bgColor = "#FEF3C7";
        borderColor = "#F59E0B";
        textColor = "#92400E";
        iconBgColor = "#FDE68A";
      } else {
        icon = "🔴";
        title = tr("远程模式 - 已断开");
        detail = m_remoteHost.isEmpty()
                     ? tr("未连接")
                     : tr("与 %1 的连接已断开").arg(m_remoteHost);
        bgColor = "#FEE2E2";
        borderColor = "#EF4444";
        textColor = "#991B1B";
        iconBgColor = "#FECACA";
      }
      break;
  }

  m_iconLabel->setText(icon);
  m_iconLabel->setStyleSheet(
      QString("QLabel { background-color: %1; border-radius: 20px; }")
          .arg(iconBgColor));

  m_titleLabel->setText(title);
  m_titleLabel->setStyleSheet(QString("QLabel { color: %1; }").arg(textColor));

  m_detailLabel->setText(detail);
  m_detailLabel->setStyleSheet(QString("QLabel { color: %1; }").arg(textColor));

  setStyleSheet(QString("#modeStatusIndicator {"
                        "  background-color: %1;"
                        "  border: 2px solid %2;"
                        "  border-radius: 10px;"
                        "}"
                        "#modeStatusIndicator:hover {"
                        "  background-color: %3;"
                        "}")
                    .arg(bgColor)
                    .arg(borderColor)
                    .arg(bgColor));
}

void ModeStatusIndicator::mousePressEvent(QMouseEvent* event) {
  Q_UNUSED(event);
  emit clicked();
}

void ModeStatusIndicator::enterEvent(QEnterEvent* event) {
  Q_UNUSED(event);
  // 悬停效果可以在这里添加
}

void ModeStatusIndicator::leaveEvent(QEvent* event) { Q_UNUSED(event); }

// ============================================================================
// MainWindow 实现
// ============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_channelManager(new ChannelManager(this)),
      m_alarmManager(new AlarmManager(m_channelManager, this)),
      m_mainSplitter(nullptr),
      m_rightSplitter(nullptr),
      m_channelScrollArea(nullptr),
      m_channelListWidget(nullptr),
      m_channelListLayout(nullptr),
      m_dashboardWidget(nullptr),
      m_messageLogPanel(nullptr),
      m_currentChannel(nullptr),
      m_channelTable(nullptr),
      m_updateTimer(new QTimer(this)),
      m_statusLabel(nullptr),
      m_channelCountLabel(nullptr),
      m_runningCountLabel(nullptr),
      m_alarmIndicator(nullptr),
      m_configModified(false),
      m_trayIcon(nullptr),
      m_trayMenu(nullptr),
      m_showHideAction(nullptr),
      m_trayExitAction(nullptr),
      m_autoStartChannels(false),
      m_logViewer(nullptr),
      m_alarmWidget(nullptr),
      m_waveformRecorder(nullptr),
      m_remoteClient(nullptr),
      m_isRemoteMode(false),
      m_connectionStatusLabel(nullptr),
      m_localApiServer(nullptr),
      m_appMode(ApplicationMode::LocalWithApi),
      m_localHttpPort(AppSettings::instance().httpPort()),
      m_localWsPort(AppSettings::instance().wsPort()),
      m_modeIndicator(nullptr) {
    setWindowTitle("Modbus PlexLink - 数据采集与虚拟化服务系统");
    resize(1200, 800);
    setMinimumSize(1000, 600);
    
    // 设置应用图标
    QIcon appIcon("icon.png");
    if (!appIcon.isNull()) {
        setWindowIcon(appIcon);
        qInfo() << "Application icon loaded from icon.png";
    } else {
        qWarning() << "Failed to load icon.png, using default icon";
    }
    
    setupUi();
    createSystemTray();

    // 初始化工具窗口与日志处理
    m_logViewer = new LogViewerWidget();
    m_logViewer->setWindowIcon(windowIcon());
    m_logViewer->hide();
    LogViewerWidget::installMessageHandler();
    
    // 连接通道管理器信号
  connect(m_channelManager, &ChannelManager::channelCreated, this,
          &MainWindow::onChannelCreated);
  connect(m_channelManager, &ChannelManager::channelDeleted, this,
          &MainWindow::onChannelDeleted);
  connect(m_channelManager, &ChannelManager::channelStateChanged, this,
          &MainWindow::onChannelStateChanged);
  connect(m_channelManager, &ChannelManager::globalError, this,
          [this](const QString& error) {
                statusBar()->showMessage(tr("错误: %1").arg(error), 10000);
                QMessageBox::warning(this, tr("通道错误"), error);
            });

    if (m_alarmManager) {
    connect(m_alarmManager, &AlarmManager::alarmTriggered, this,
            &MainWindow::onAlarmTriggered, Qt::QueuedConnection);
    connect(
        m_alarmManager, &AlarmManager::alarmAcknowledged, this,
        [this](const QString&) { updateAlarmIndicator(); },
        Qt::QueuedConnection);
    connect(
        m_alarmManager, &AlarmManager::alarmCleared, this,
        [this](const QString&) { updateAlarmIndicator(); },
        Qt::QueuedConnection);
    }
    
    // 定时更新界面
  connect(m_updateTimer, &QTimer::timeout, this,
          &MainWindow::updateChannelTable);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_updateTimer->start(1000);  // 每秒更新
    
    // 加载设置
    QSettings settings("ModbusPlexLink", "MainWindow");
    m_autoStartChannels = settings.value("autoStartChannels", false).toBool();
    
    // 尝试加载默认配置
    QString defaultConfig = "config.json";
    if (QFile::exists(defaultConfig)) {
        if (m_channelManager->loadConfig(defaultConfig)) {
            m_currentConfigFile = defaultConfig;
            m_configModified = false;
            statusBar()->showMessage(tr("已加载配置: %1").arg(defaultConfig), 5000);
        }
    }

    // 加载报警配置
    QString alarmConfig = "alarm_config.json";
    if (QFile::exists(alarmConfig) && m_alarmManager) {
        if (m_alarmManager->loadConfig(alarmConfig)) {
            qInfo() << "Alarm configuration loaded from" << alarmConfig;
        } else {
            qWarning() << "Failed to load alarm configuration from" << alarmConfig;
        }
    }
    
    updateChannelTable();
    updateActions();
    updateAlarmIndicator();

  // 初始化运行模式（默认：本地+远程API）
  initializeMode(ApplicationMode::LocalWithApi);
    
    // 如果设置了自动启动，延迟启动所有通道
    if (m_autoStartChannels) {
        QTimer::singleShot(1000, this, &MainWindow::autoStartAllChannels);
    }
}

MainWindow::~MainWindow() {
  // 停止本地API服务器
  stopLocalApiServer();

    // 自动保存报警配置
    if (m_alarmManager) {
        QString alarmConfig = "alarm_config.json";
        if (m_alarmManager->saveConfig(alarmConfig)) {
            qInfo() << "Alarm configuration saved to" << alarmConfig;
        }
    }

    if (m_alarmWidget) {
        m_alarmWidget->close();
        delete m_alarmWidget;
        m_alarmWidget = nullptr;
    }

    if (m_waveformRecorder) {
        m_waveformRecorder->close();
        delete m_waveformRecorder;
        m_waveformRecorder = nullptr;
    }

    if (m_logViewer) {
        m_logViewer->close();
        delete m_logViewer;
        m_logViewer = nullptr;
    }

    LogViewerWidget::uninstallMessageHandler();
    m_channelManager->stopAll();
}

// ============================================================================
// 模式管理
// ============================================================================

void MainWindow::initializeMode(ApplicationMode mode) {
  m_appMode = mode;

  switch (mode) {
    case ApplicationMode::LocalWithApi:
      // 本地采集 + 远程API服务
      startLocalApiServer();
      m_isRemoteMode = false;
      if (m_modeIndicator) {
        m_modeIndicator->setLocalMode(true, m_localHttpPort, m_localWsPort);
      }
      setWindowTitle("Modbus PlexLink - 数据采集与虚拟化服务系统");
      break;

    case ApplicationMode::LocalOnly:
      // 仅本地采集
      stopLocalApiServer();
      m_isRemoteMode = false;
      if (m_modeIndicator) {
        m_modeIndicator->setLocalMode(false);
      }
      setWindowTitle("Modbus PlexLink - 本地模式");
      break;

    case ApplicationMode::RemoteClient:
      // 远程客户端模式
      stopLocalApiServer();
      m_isRemoteMode = true;
      if (m_modeIndicator) {
        m_modeIndicator->setRemoteMode("", false);
      }
      setWindowTitle("Modbus PlexLink - 远程客户端");
      break;
  }

  updateStatusBar();
  qInfo() << "Application mode initialized:" << static_cast<int>(mode);
}

void MainWindow::startLocalApiServer() {
  if (m_localApiServer) {
    qDebug() << "Local API server already running";
    return;
  }

  m_localApiServer =
      new RemoteApiServer(m_channelManager, m_alarmManager, this);

  // 连接信号
  connect(m_localApiServer, &RemoteApiServer::clientConnected, this,
          [this](const QString& id) {
            qInfo() << "[LocalAPI] 客户端已连接:" << id;
            statusBar()->showMessage(tr("远程客户端已连接: %1").arg(id), 3000);
          });

  connect(m_localApiServer, &RemoteApiServer::clientDisconnected, this,
          [this](const QString& id) {
            qInfo() << "[LocalAPI] 客户端已断开:" << id;
          });

  connect(m_localApiServer, &RemoteApiServer::serverError, this,
          [this](const QString& error) {
            qWarning() << "[LocalAPI] 错误:" << error;
          });

  // 启动服务器
  if (m_localApiServer->start(m_localHttpPort, m_localWsPort)) {
    qInfo() << "[LocalAPI] 本地API服务器已启动 - HTTP:" << m_localHttpPort
            << "WS:" << m_localWsPort;
    if (m_modeIndicator) {
      m_modeIndicator->setLocalMode(true, m_localHttpPort, m_localWsPort);
    }
  } else {
    qWarning() << "[LocalAPI] 本地API服务器启动失败";
    delete m_localApiServer;
    m_localApiServer = nullptr;

    QMessageBox::warning(this, tr("API服务器启动失败"),
                         tr("无法启动本地API服务器（端口 %1/%2 可能被占用）。\n"
                            "将以纯本地模式运行。")
                             .arg(m_localHttpPort)
                             .arg(m_localWsPort));

    if (m_modeIndicator) {
      m_modeIndicator->setLocalMode(false);
    }
  }
}

void MainWindow::stopLocalApiServer() {
  if (m_localApiServer) {
    m_localApiServer->stop();
    delete m_localApiServer;
    m_localApiServer = nullptr;
    qInfo() << "[LocalAPI] 本地API服务器已停止";
  }
}

void MainWindow::enterRemoteMode() {
  m_isRemoteMode = true;
  m_appMode = ApplicationMode::RemoteClient;

  // *** 重要：断开当前通道的信号连接，避免重复显示报文 ***
  if (m_currentChannel) {
    disconnect(m_currentChannel, nullptr, this, nullptr);
    if (m_messageLogPanel && m_messageLogPanel->getMessageViewer()) {
      disconnect(m_currentChannel, nullptr, m_messageLogPanel->getMessageViewer(), nullptr);
    }
    m_currentChannel = nullptr;
  }

  // *** 重要：停止所有本地通道，避免继续产生报文 ***
  if (m_channelManager) {
    int runningCount = 0;
    for (Channel* ch : m_channelManager->getAllChannels()) {
      if (ch && ch->isRunning()) {
        runningCount++;
      }
    }
    if (runningCount > 0) {
      qInfo() << "Stopping" << runningCount
              << "local channels before entering remote mode";
      m_channelManager->stopAll();
    }
  }

  // 清除本地通道显示
  clearChannelCards();

  // 设置仪表盘为远程模式
  if (m_dashboardWidget) {
    m_dashboardWidget->setRemoteMode(true);
  }

  // 设置报文面板为远程模式（清除本地报文，准备接收远程报文）
  if (m_messageLogPanel) {
    m_messageLogPanel->setRemoteMode(true);
  }

  m_currentChannel = nullptr;

  qInfo() << "Entered remote mode (local channels stopped)";
}

void MainWindow::exitRemoteMode() {
  m_isRemoteMode = false;
  m_remoteChannelsCache = QJsonArray();
  m_remoteDataCache.clear();

  // 恢复仪表盘为本地模式
  if (m_dashboardWidget) {
    m_dashboardWidget->setRemoteMode(false);
  }

  // 恢复报文面板为本地模式（清除远程报文）
  if (m_messageLogPanel) {
    m_messageLogPanel->setRemoteMode(false);
  }

  // 恢复本地通道显示
  updateChannelCards();

  // 选择第一个本地通道
  QList<Channel*> channels = m_channelManager->getAllChannels();
  if (!channels.isEmpty()) {
    selectChannel(channels.first());
  }

  qInfo() << "Exited remote mode, back to local (local channels remain "
             "stopped, use 'Start All' to restart)";
}

void MainWindow::clearChannelCards() {
  // 清除现有卡片
  qDeleteAll(m_channelCards);
  m_channelCards.clear();

  // 清空布局
  QLayoutItem* item;
  while ((item = m_channelListLayout->takeAt(0)) != nullptr) {
    delete item->widget();
    delete item;
  }
}

void MainWindow::updateRemoteChannelCards() {
  clearChannelCards();

  if (m_remoteChannelsCache.isEmpty()) {
    // 显示空状态
    QLabel* emptyLabel = new QLabel(
        tr("🌐 已连接到远程服务\n\n远程服务器暂无通道"), m_channelListWidget);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet(R"(
            QLabel {
                color: #9CA3AF;
                font-size: 12px;
                padding: 40px 20px;
            }
        )");
    m_channelListLayout->addWidget(emptyLabel);
  } else {
    // 为每个远程通道创建一个简化的卡片
    for (const QJsonValue& val : m_remoteChannelsCache) {
      QJsonObject ch = val.toObject();
      QString name = ch["name"].toString();
      bool running = ch["running"].toBool();
      int collectorCount = ch["collectorCount"].toInt();
      int serverCount = ch["serverCount"].toInt();

      // 创建简化的远程通道卡片
      QFrame* card = new QFrame(m_channelListWidget);
      card->setObjectName("remoteChannelCard");
      card->setStyleSheet(R"(
                #remoteChannelCard {
                    background-color: white;
                    border: 1px solid #E5E7EB;
                    border-radius: 8px;
                    padding: 12px;
                }
                #remoteChannelCard:hover {
                    border-color: #F59E0B;
                    background-color: #FFFBEB;
                }
            )");

      QVBoxLayout* cardLayout = new QVBoxLayout(card);
      cardLayout->setContentsMargins(12, 10, 12, 10);
      cardLayout->setSpacing(6);

      // 通道名称行
      QHBoxLayout* nameRow = new QHBoxLayout();
      QLabel* nameLabel = new QLabel(name, card);
      QFont nameFont = nameLabel->font();
      nameFont.setBold(true);
      nameFont.setPointSize(11);
      nameLabel->setFont(nameFont);
      nameRow->addWidget(nameLabel);

      // 远程标记
      QLabel* remoteTag = new QLabel("🌐", card);
      remoteTag->setToolTip(tr("远程通道"));
      nameRow->addWidget(remoteTag);
      nameRow->addStretch();

      // 状态指示器
      QLabel* statusDot = new QLabel(card);
      statusDot->setFixedSize(10, 10);
      statusDot->setStyleSheet(
          QString("background-color: %1; border-radius: 5px;")
              .arg(running ? "#10B981" : "#9CA3AF"));
      nameRow->addWidget(statusDot);

      cardLayout->addLayout(nameRow);

      // 状态文字
      QLabel* statusLabel =
          new QLabel(running ? tr("运行中") : tr("已停止"), card);
      statusLabel->setStyleSheet(
          QString("color: %1;").arg(running ? "#059669" : "#6B7280"));
      cardLayout->addWidget(statusLabel);

      // 统计信息
      QLabel* statsLabel = new QLabel(
          tr("采集器: %1 | 服务器: %2").arg(collectorCount).arg(serverCount),
          card);
      statsLabel->setStyleSheet("color: #9CA3AF; font-size: 10px;");
      cardLayout->addWidget(statsLabel);

      // 操作按钮行（与本地卡片保持一致：启动/停止、编辑、删除）
      QHBoxLayout* buttonRow = new QHBoxLayout();
      buttonRow->setSpacing(4);
      buttonRow->addStretch();

      // 启动/停止按钮
      QPushButton* startStopBtn = new QPushButton(running ? "⏹" : "▶", card);
      startStopBtn->setObjectName("iconButton");
      startStopBtn->setFixedSize(28, 28);
      startStopBtn->setCursor(Qt::PointingHandCursor);
      startStopBtn->setToolTip(running ? tr("停止通道") : tr("启动通道"));
      startStopBtn->setProperty("channelName", name);
      startStopBtn->setProperty("isRunning", running);
      startStopBtn->setStyleSheet(R"(
                QPushButton#iconButton {
                    background-color: #F3F4F6;
                    border: 1px solid #E5E7EB;
                    border-radius: 4px;
                    font-size: 12px;
                }
                QPushButton#iconButton:hover {
                    background-color: #E5E7EB;
                    border-color: #D1D5DB;
                }
            )");
      connect(
          startStopBtn, &QPushButton::clicked, this, [this, startStopBtn]() {
            QString chName = startStopBtn->property("channelName").toString();
            bool isRunning = startStopBtn->property("isRunning").toBool();
            if (m_remoteClient && m_remoteClient->isConnected()) {
              if (isRunning) {
                m_remoteClient->stopChannel(chName);
                statusBar()->showMessage(
                    tr("正在停止远程通道 '%1'...").arg(chName), 3000);
              } else {
                m_remoteClient->startChannel(chName);
                statusBar()->showMessage(
                    tr("正在启动远程通道 '%1'...").arg(chName), 3000);
              }
              // 延迟刷新通道列表
              QTimer::singleShot(500, this, [this]() {
                if (m_remoteClient && m_remoteClient->isConnected()) {
                  m_remoteClient->getChannels();
                }
              });
            }
          });
      buttonRow->addWidget(startStopBtn);

      // 编辑按钮
      QPushButton* editBtn = new QPushButton("✎", card);
      editBtn->setObjectName("iconButton");
      editBtn->setFixedSize(28, 28);
      editBtn->setCursor(Qt::PointingHandCursor);
      editBtn->setToolTip(tr("编辑通道"));
      editBtn->setProperty("channelName", name);
      editBtn->setStyleSheet(R"(
                QPushButton#iconButton {
                    background-color: #F3F4F6;
                    border: 1px solid #E5E7EB;
                    border-radius: 4px;
                    font-size: 12px;
                }
                QPushButton#iconButton:hover {
                    background-color: #E5E7EB;
                    border-color: #D1D5DB;
                }
            )");
      connect(editBtn, &QPushButton::clicked, this, [this, editBtn]() {
        QString chName = editBtn->property("channelName").toString();
        // 先选中该通道
        if (m_dashboardWidget) {
          m_dashboardWidget->setRemoteMode(true, chName);
        }
        // 请求远程通道配置，然后打开编辑对话框
        if (m_remoteClient && m_remoteClient->isConnected()) {
          m_pendingEditRemoteChannel = chName;
          m_remoteClient->getChannel(chName);
          statusBar()->showMessage(tr("正在获取远程通道 '%1' 的配置...").arg(chName), 3000);
        }
      });
      buttonRow->addWidget(editBtn);

      // 删除按钮
      QPushButton* deleteBtn = new QPushButton("🗑", card);
      deleteBtn->setObjectName("iconButton");
      deleteBtn->setFixedSize(28, 28);
      deleteBtn->setCursor(Qt::PointingHandCursor);
      deleteBtn->setToolTip(tr("删除通道"));
      deleteBtn->setProperty("channelName", name);
      deleteBtn->setStyleSheet(R"(
                QPushButton#iconButton {
                    background-color: #F3F4F6;
                    border: 1px solid #E5E7EB;
                    border-radius: 4px;
                    font-size: 12px;
                }
                QPushButton#iconButton:hover {
                    background-color: #FEE2E2;
                    border-color: #FECACA;
                }
            )");
      connect(deleteBtn, &QPushButton::clicked, this, [this, deleteBtn]() {
        QString chName = deleteBtn->property("channelName").toString();
        if (m_remoteClient && m_remoteClient->isConnected()) {
          QMessageBox::StandardButton reply = QMessageBox::question(
              this, tr("确认删除远程通道"),
              tr("确定要删除远程通道 '%1' "
                 "吗？\n\n此操作将删除远程服务器上的通道配置。")
                  .arg(chName),
              QMessageBox::Yes | QMessageBox::No);

          if (reply == QMessageBox::Yes) {
            m_remoteClient->deleteChannel(chName);
            statusBar()->showMessage(tr("正在删除远程通道 '%1'...").arg(chName),
                                     3000);
            // 如果删除的是当前选中的通道，清除选中状态
            if (m_dashboardWidget &&
                m_dashboardWidget->getRemoteChannelName() == chName) {
              m_dashboardWidget->setRemoteMode(true, "");
            }
            // 延迟刷新通道列表
            QTimer::singleShot(500, this, [this]() {
              if (m_remoteClient && m_remoteClient->isConnected()) {
                m_remoteClient->getChannels();
              }
            });
          }
        }
      });
      buttonRow->addWidget(deleteBtn);

      cardLayout->addLayout(buttonRow);

      // 点击事件（选择通道查看数据）
      card->setCursor(Qt::PointingHandCursor);
      card->setProperty("channelName", name);
      card->installEventFilter(this);

      m_channelListLayout->addWidget(card);
    }
  }

  // 添加弹簧
  m_channelListLayout->addStretch();
}

void MainWindow::setupUi() {
    createActions();
    createMenus();
    createToolBar();
    createCentralWidget();
    createStatusBar();
}

void MainWindow::createActions() {
    // 文件操作
    m_newChannelAction = new QAction(tr("➕ 新建通道"), this);
    m_newChannelAction->setShortcut(QKeySequence::New);
    m_newChannelAction->setStatusTip(tr("创建新的采集转发通道"));
  connect(m_newChannelAction, &QAction::triggered, this,
          &MainWindow::onNewChannel);
    
    m_loadConfigAction = new QAction(tr("📂 打开"), this);
    m_loadConfigAction->setShortcut(QKeySequence::Open);
    m_loadConfigAction->setStatusTip(tr("从文件加载配置"));
  connect(m_loadConfigAction, &QAction::triggered, this,
          &MainWindow::onLoadConfig);
    
    m_saveConfigAction = new QAction(tr("💾 保存"), this);
    m_saveConfigAction->setShortcut(QKeySequence::Save);
    m_saveConfigAction->setStatusTip(tr("保存当前配置"));
  connect(m_saveConfigAction, &QAction::triggered, this,
          &MainWindow::onSaveConfig);
    
    m_saveConfigAsAction = new QAction(tr("另存为..."), this);
    m_saveConfigAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveConfigAsAction->setStatusTip(tr("保存配置到新文件"));
  connect(m_saveConfigAsAction, &QAction::triggered, this,
          &MainWindow::onSaveConfigAs);
    
    m_exitAction = new QAction(tr("退出"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip(tr("退出应用程序"));
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::onExit);
    
    // 通道操作
    m_editChannelAction = new QAction(tr("编辑"), this);
    m_editChannelAction->setStatusTip(tr("编辑选中的通道配置"));
  connect(m_editChannelAction, &QAction::triggered, this,
          &MainWindow::onEditChannel);
    
    m_deleteChannelAction = new QAction(tr("删除"), this);
    m_deleteChannelAction->setStatusTip(tr("删除选中的通道"));
  connect(m_deleteChannelAction, &QAction::triggered, this,
          &MainWindow::onDeleteChannel);
    
    m_startChannelAction = new QAction(tr("启动"), this);
    m_startChannelAction->setStatusTip(tr("启动选中的通道"));
  connect(m_startChannelAction, &QAction::triggered, this,
          &MainWindow::onStartChannel);
    
    m_stopChannelAction = new QAction(tr("停止"), this);
    m_stopChannelAction->setStatusTip(tr("停止选中的通道"));
  connect(m_stopChannelAction, &QAction::triggered, this,
          &MainWindow::onStopChannel);
    
    m_startAllAction = new QAction(tr("▶ 全部启动"), this);
    m_startAllAction->setStatusTip(tr("启动所有通道"));
  connect(m_startAllAction, &QAction::triggered, this,
          &MainWindow::onStartAllChannels);
    
    m_stopAllAction = new QAction(tr("⏹ 全部停止"), this);
    m_stopAllAction->setStatusTip(tr("停止所有通道"));
  connect(m_stopAllAction, &QAction::triggered, this,
          &MainWindow::onStopAllChannels);
    
    // 帮助
    m_aboutAction = new QAction(tr("ℹ 关于"), this);
    m_aboutAction->setStatusTip(tr("关于 Modbus PlexLink"));
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    // 工具
    m_logViewerAction = new QAction(tr("📋 日志"), this);
    m_logViewerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    m_logViewerAction->setStatusTip(tr("查看系统运行日志"));
  connect(m_logViewerAction, &QAction::triggered, this,
          &MainWindow::onShowLogViewer);

    m_alarmManagerAction = new QAction(tr("🔔 报警"), this);
  m_alarmManagerAction->setShortcut(
      QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A));
    m_alarmManagerAction->setStatusTip(tr("打开报警管理窗口"));
  connect(m_alarmManagerAction, &QAction::triggered, this,
          &MainWindow::onShowAlarmManager);
    
    m_waveformRecorderAction = new QAction(tr("📈 录波"), this);
    m_waveformRecorderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    m_waveformRecorderAction->setStatusTip(tr("打开指定点位录波窗口"));
  connect(m_waveformRecorderAction, &QAction::triggered, this,
          &MainWindow::onShowWaveformRecorder);
    
    // 设置 - 自动启动选项
    QAction* autoStartAction = new QAction(tr("启动时自动运行所有通道"), this);
    autoStartAction->setCheckable(true);
    autoStartAction->setChecked(m_autoStartChannels);
  connect(autoStartAction, &QAction::triggered, this,
          [this, autoStartAction]() {
        m_autoStartChannels = autoStartAction->isChecked();
        QSettings settings("ModbusPlexLink", "MainWindow");
        settings.setValue("autoStartChannels", m_autoStartChannels);
            statusBar()->showMessage(m_autoStartChannels
                                         ? tr("已启用：启动时自动运行所有通道")
                                         : tr("已禁用：启动时自动运行所有通道"),
            3000);
    });
    
    // 将设置Action添加到成员变量以供菜单使用
    m_aboutAction->setData(QVariant::fromValue(autoStartAction));
    
    // 远程连接
    m_connectRemoteAction = new QAction(tr("🌐 远程连接"), this);
    m_connectRemoteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_connectRemoteAction->setStatusTip(tr("连接到远程边缘网关服务"));
  connect(m_connectRemoteAction, &QAction::triggered, this,
          &MainWindow::onConnectRemote);
    
    m_disconnectRemoteAction = new QAction(tr("🔌 断开远程"), this);
    m_disconnectRemoteAction->setStatusTip(tr("断开与远程服务的连接"));
    m_disconnectRemoteAction->setEnabled(false);
  connect(m_disconnectRemoteAction, &QAction::triggered, this,
          &MainWindow::onDisconnectRemote);
    
    m_switchToLocalAction = new QAction(tr("💻 本地模式"), this);
    m_switchToLocalAction->setStatusTip(tr("切换回本地采集模式"));
    m_switchToLocalAction->setEnabled(false);
  connect(m_switchToLocalAction, &QAction::triggered, this,
          &MainWindow::onSwitchToLocalMode);

  // 应用设置
  m_appSettingsAction = new QAction(tr("⚙️ 应用设置"), this);
  m_appSettingsAction->setStatusTip(tr("配置API端口、认证和启动行为"));
  connect(m_appSettingsAction, &QAction::triggered, this,
          &MainWindow::onShowAppSettings);
}

void MainWindow::createMenus() {
    // 隐藏菜单栏，功能已整合到工具栏
    menuBar()->hide();
}

void MainWindow::createToolBar() {
    QToolBar* toolBar = addToolBar(tr("主工具栏"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(24, 24));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    
    // 文件操作
    toolBar->addAction(m_newChannelAction);
    toolBar->addAction(m_loadConfigAction);
    toolBar->addAction(m_saveConfigAction);
    toolBar->addSeparator();
    
    // 通道操作
    toolBar->addAction(m_startAllAction);
    toolBar->addAction(m_stopAllAction);
    toolBar->addSeparator();
    
    // 工具
    toolBar->addAction(m_logViewerAction);
    toolBar->addAction(m_alarmManagerAction);
    toolBar->addAction(m_waveformRecorderAction);
    toolBar->addSeparator();
    
    // 远程连接
    toolBar->addAction(m_connectRemoteAction);
    toolBar->addAction(m_disconnectRemoteAction);
    toolBar->addAction(m_switchToLocalAction);
  toolBar->addSeparator();

  // 应用设置
  toolBar->addAction(m_appSettingsAction);
    
    // 添加弹性空间
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);
    
    // 关于按钮
    toolBar->addAction(m_aboutAction);
}

void MainWindow::createCentralWidget() {
    // 创建主分割器（水平分割：左侧通道列表 | 右侧数据+日志）
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setObjectName("mainSplitter");
    m_mainSplitter->setChildrenCollapsible(false);

    // ============ 左侧：通道卡片列表 ============
    QWidget* leftPanel = new QWidget(this);
    leftPanel->setObjectName("leftPanel");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(12);

  // ★ 醒目的模式状态指示器 ★
  m_modeIndicator = new ModeStatusIndicator(leftPanel);
  connect(m_modeIndicator, &ModeStatusIndicator::clicked, this, [this]() {
    // 点击模式指示器时显示模式切换菜单
    QMenu menu(this);
    menu.setStyleSheet(R"(
            QMenu {
                background-color: white;
                border: 1px solid #E5E7EB;
                border-radius: 8px;
                padding: 8px;
            }
            QMenu::item {
                padding: 8px 16px;
                border-radius: 4px;
            }
            QMenu::item:selected {
                background-color: #F3F4F6;
            }
        )");

    QAction* localApiAction = menu.addAction(tr("🖥️ 本地模式 + 远程API"));
    localApiAction->setCheckable(true);
    localApiAction->setChecked(m_appMode == ApplicationMode::LocalWithApi);

    QAction* localOnlyAction = menu.addAction(tr("💻 本地模式（无API）"));
    localOnlyAction->setCheckable(true);
    localOnlyAction->setChecked(m_appMode == ApplicationMode::LocalOnly);

    menu.addSeparator();

    QAction* connectRemoteAction = menu.addAction(tr("🌐 连接远程服务..."));

    if (m_isRemoteMode && m_remoteClient && m_remoteClient->isConnected()) {
      menu.addSeparator();
      QAction* disconnectAction = menu.addAction(tr("🔌 断开远程连接"));
      connect(disconnectAction, &QAction::triggered, this,
              &MainWindow::onDisconnectRemote);
    }

    connect(localApiAction, &QAction::triggered, this, [this]() {
      if (m_isRemoteMode) {
        onSwitchToLocalMode();
      }
      initializeMode(ApplicationMode::LocalWithApi);
    });

    connect(localOnlyAction, &QAction::triggered, this, [this]() {
      if (m_isRemoteMode) {
        onSwitchToLocalMode();
      }
      initializeMode(ApplicationMode::LocalOnly);
    });

    connect(connectRemoteAction, &QAction::triggered, this,
            &MainWindow::onConnectRemote);

    menu.exec(
        m_modeIndicator->mapToGlobal(QPoint(0, m_modeIndicator->height())));
  });
  leftLayout->addWidget(m_modeIndicator);

    // 标题和操作按钮
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(tr("📋 通道列表"), leftPanel);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    // 打开配置按钮
    QPushButton* openConfigBtn = new QPushButton("📂", leftPanel);
    openConfigBtn->setObjectName("iconButtonHeader");
    openConfigBtn->setFixedSize(28, 28);
    openConfigBtn->setCursor(Qt::PointingHandCursor);
    openConfigBtn->setToolTip(tr("打开配置文件"));
  connect(openConfigBtn, &QPushButton::clicked, this,
          &MainWindow::onLoadConfig);
    headerLayout->addWidget(openConfigBtn);

    // 保存配置按钮
    QPushButton* saveConfigBtn = new QPushButton("💾", leftPanel);
    saveConfigBtn->setObjectName("iconButtonHeader");
    saveConfigBtn->setFixedSize(28, 28);
    saveConfigBtn->setCursor(Qt::PointingHandCursor);
    saveConfigBtn->setToolTip(tr("保存配置"));
  connect(saveConfigBtn, &QPushButton::clicked, this,
          &MainWindow::onSaveConfig);
    headerLayout->addWidget(saveConfigBtn);

    // 新建通道按钮
    QPushButton* addChannelBtn = new QPushButton("➕", leftPanel);
    addChannelBtn->setObjectName("iconButtonHeader");
    addChannelBtn->setFixedSize(28, 28);
    addChannelBtn->setCursor(Qt::PointingHandCursor);
    addChannelBtn->setToolTip(tr("新建通道"));
  connect(addChannelBtn, &QPushButton::clicked, this,
          &MainWindow::onNewChannel);
    headerLayout->addWidget(addChannelBtn);

    leftLayout->addLayout(headerLayout);

    // 通道卡片滚动区域
    m_channelScrollArea = new QScrollArea(leftPanel);
    m_channelScrollArea->setWidgetResizable(true);
    m_channelScrollArea->setFrameShape(QFrame::NoFrame);
    m_channelScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_channelScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_channelListWidget = new QWidget();
    m_channelListLayout = new QVBoxLayout(m_channelListWidget);
    m_channelListLayout->setContentsMargins(0, 0, 0, 0);
    m_channelListLayout->setSpacing(8);
    m_channelListLayout->setAlignment(Qt::AlignTop);

    m_channelScrollArea->setWidget(m_channelListWidget);
    leftLayout->addWidget(m_channelScrollArea);

    // 左侧面板最小宽度
    leftPanel->setMinimumWidth(280);
    m_mainSplitter->addWidget(leftPanel);

    // ============ 右侧：上下分割（数据仪表盘 | 报文日志） ============
    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    m_rightSplitter->setObjectName("rightSplitter");
    m_rightSplitter->setChildrenCollapsible(false);

    // 右上：仪表盘数据展示
    QWidget* dashboardPanel = new QWidget(this);
    dashboardPanel->setObjectName("dashboardPanel");
    QVBoxLayout* dashboardLayout = new QVBoxLayout(dashboardPanel);
    dashboardLayout->setContentsMargins(0, 0, 0, 0);
    dashboardLayout->setSpacing(0);

    m_dashboardWidget = new DashboardDataWidget(dashboardPanel);
    dashboardLayout->addWidget(m_dashboardWidget);

    m_rightSplitter->addWidget(dashboardPanel);

    // 右下：报文与日志面板
    m_messageLogPanel = new MessageLogPanel(this);
    m_messageLogPanel->setMinimumHeight(260);  // 设置最小高度为260px
    m_rightSplitter->addWidget(m_messageLogPanel);

    // 设置右侧上下分割比例 (60% : 40%)
    m_rightSplitter->setStretchFactor(0, 60);
    m_rightSplitter->setStretchFactor(1, 40);

    m_mainSplitter->addWidget(m_rightSplitter);

    // 设置主分割器左右比例 (30% : 70%)
    m_mainSplitter->setStretchFactor(0, 30);
    m_mainSplitter->setStretchFactor(1, 70);

    setCentralWidget(m_mainSplitter);
    
    // 设置右侧splitter的初始大小，使右下区域初始高度为260px
    // 假设窗口高度约为800px，右侧区域高度约为600px，则右上约为340px，右下为260px
    QList<int> rightSizes;
    rightSizes << 340 << 260;  // 右上340px，右下260px
    m_rightSplitter->setSizes(rightSizes);

    // 应用现代化主题
    applyModernTheme();

    // 初始化通道卡片列表
    updateChannelCards();
}

void MainWindow::createStatusBar() {
    m_statusLabel = new QLabel(tr("就绪"), this);
    m_channelCountLabel = new QLabel(tr("通道: 0"), this);
    m_runningCountLabel = new QLabel(tr("运行: 0"), this);
    m_alarmIndicator = new QLabel(tr("报警: 0"), this);
  m_alarmIndicator->setStyleSheet(
      "padding: 0 10px; font-weight: bold; color: #2e7d32;");
    m_alarmIndicator->setCursor(Qt::PointingHandCursor);
    m_alarmIndicator->installEventFilter(this);
    
    m_connectionStatusLabel = new QLabel(tr("本地模式"), this);
    m_connectionStatusLabel->setStyleSheet("padding: 0 10px; color: #6B7280;");
    
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_connectionStatusLabel);
    statusBar()->addPermanentWidget(m_channelCountLabel);
    statusBar()->addPermanentWidget(m_runningCountLabel);
    statusBar()->addPermanentWidget(m_alarmIndicator);
}

void MainWindow::updateActions() {
    QString selectedChannel = getSelectedChannelName();
    bool hasSelection = !selectedChannel.isEmpty();
    bool hasChannels = m_channelManager->getChannelCount() > 0;
    
    m_editChannelAction->setEnabled(hasSelection);
    m_deleteChannelAction->setEnabled(hasSelection);
    m_startChannelAction->setEnabled(hasSelection);
    m_stopChannelAction->setEnabled(hasSelection);
    
    m_startAllAction->setEnabled(hasChannels);
    m_stopAllAction->setEnabled(hasChannels);
    
    m_saveConfigAction->setEnabled(hasChannels);
    m_saveConfigAsAction->setEnabled(hasChannels);
}

QString MainWindow::channelStateToString(ChannelState state) const {
    switch (state) {
    case ChannelState::Stopped:
      return tr("已停止");
    case ChannelState::Starting:
      return tr("启动中");
    case ChannelState::Running:
      return tr("运行中");
    case ChannelState::Stopping:
      return tr("停止中");
    case ChannelState::Error:
      return tr("错误");
    default:
      return tr("未知");
    }
}

QColor MainWindow::channelStateColor(ChannelState state) const {
    switch (state) {
    case ChannelState::Stopped:
      return QColor(150, 150, 150);
    case ChannelState::Starting:
      return QColor(255, 165, 0);
    case ChannelState::Running:
      return QColor(34, 139, 34);
    case ChannelState::Stopping:
      return QColor(255, 165, 0);
    case ChannelState::Error:
      return QColor(220, 20, 60);
    default:
      return QColor(0, 0, 0);
    }
}

QString MainWindow::getChannelConnectionStatus(Channel* channel) const {
    if (!channel) {
        return tr("N/A");
    }
    
    // 获取采集器连接状态
    QList<ICollector*> collectors = channel->getCollectors();
    int collectorTotal = collectors.size();
    int collectorConnected = 0;
    
    for (ICollector* collector : collectors) {
        if (collector && collector->isConnected()) {
            collectorConnected++;
        }
    }
    
    // 获取服务器连接状态
    QList<IServer*> servers = channel->getServers();
    int serverTotal = servers.size();
    int serverRunning = 0;
    int totalClients = 0;
    
    for (IServer* server : servers) {
        if (server && server->isRunning()) {
            serverRunning++;
            totalClients += server->getClientCount();
        }
    }
    
    // 构建状态字符串
    QString status;
    if (collectorTotal > 0) {
        status += tr("采集器: %1/%2").arg(collectorConnected).arg(collectorTotal);
    }
    
    if (serverTotal > 0) {
        if (!status.isEmpty()) {
            status += " | ";
        }
        status += tr("服务器: %1/%2").arg(serverRunning).arg(serverTotal);
        if (totalClients > 0) {
            status += tr(" (%1客户端)").arg(totalClients);
        }
    }
    
    if (status.isEmpty()) {
        status = tr("无设备");
    }
    
    return status;
}

void MainWindow::updateAlarmIndicator() {
    if (!m_alarmIndicator) {
        return;
    }

    if (!m_alarmManager) {
        m_alarmIndicator->setText(tr("报警: -"));
    m_alarmIndicator->setStyleSheet(
        "padding: 0 10px; font-weight: bold; color: #757575;");
        return;
    }

    int activeAlarms = m_alarmManager->getActiveAlarmCount();
    int criticalAlarms = m_alarmManager->getAlarmCount(AlarmPriority::Critical);

    QString color;
    QString text;
    if (activeAlarms == 0) {
        text = tr("报警: 0 (正常)");
        color = "#2e7d32";
    } else if (criticalAlarms > 0) {
        text = tr("报警: %1 (严重)").arg(activeAlarms);
        color = "#b71c1c";
    } else {
        text = tr("报警: %1").arg(activeAlarms);
        color = "#e67e22";
    }

    m_alarmIndicator->setText(text);
    m_alarmIndicator->setStyleSheet(
      QStringLiteral("padding: 0 10px; font-weight: bold; color: %1;")
          .arg(color));
}

void MainWindow::updateStatusBar() {
  if (m_isRemoteMode) {
    // 远程模式：使用缓存的远程通道数据
    int totalChannels = m_remoteChannelsCache.size();
    int runningChannels = 0;
    for (const QJsonValue& val : m_remoteChannelsCache) {
      if (val.toObject()["running"].toBool()) {
        runningChannels++;
      }
    }
    m_channelCountLabel->setText(tr("远程通道: %1").arg(totalChannels));
    m_runningCountLabel->setText(tr("运行: %1").arg(runningChannels));
  } else {
    // 本地模式
    int totalChannels = m_channelManager->getChannelCount();
    int runningChannels = 0;
    
    for (const QString& name : m_channelManager->getChannelNames()) {
        Channel* channel = m_channelManager->getChannel(name);
        if (channel && channel->isRunning()) {
            runningChannels++;
        }
    }
    
    m_channelCountLabel->setText(tr("通道: %1").arg(totalChannels));
    m_runningCountLabel->setText(tr("运行: %1").arg(runningChannels));
  }
    updateAlarmIndicator();
}

void MainWindow::onNewChannel() {
  if (m_isRemoteMode) {
    // 远程模式：创建远程通道（复用本地对话框）
    if (!m_remoteClient || !m_remoteClient->isConnected()) {
      QMessageBox::warning(this, tr("未连接"), tr("请先连接到远程服务"));
      return;
    }
    
    // 使用与本地模式相同的对话框
    ChannelConfigDialog dialog(nullptr, m_channelManager, this);
    dialog.setWindowTitle(tr("新建远程通道"));

    if (dialog.exec() == QDialog::Accepted) {
      ChannelConfig config = dialog.getConfig();
      
      // 将配置转换为 JSON
      QJsonObject configJson;
      configJson["name"] = config.name;
      configJson["enabled"] = config.enabled;
      configJson["description"] = config.description;
      
      // 采集器配置
      QJsonArray collectorsArray;
      for (const QJsonObject& collector : config.collectors) {
        collectorsArray.append(collector);
      }
      configJson["collectors"] = collectorsArray;
      
      // 服务器配置
      QJsonArray serversArray;
      for (const QJsonObject& server : config.servers) {
        serversArray.append(server);
      }
      configJson["servers"] = serversArray;
      
      // 通过 API 创建远程通道
      m_remoteClient->createChannel(configJson);
      statusBar()->showMessage(tr("正在创建远程通道 '%1'...").arg(config.name), 3000);
      
      // 延迟刷新通道列表
      QTimer::singleShot(500, this, [this]() {
        if (m_remoteClient && m_remoteClient->isConnected()) {
          m_remoteClient->getChannels();
        }
      });
    }
  } else {
    // 本地模式：创建本地通道
    ChannelConfigDialog dialog(nullptr, m_channelManager, this);
    dialog.setWindowTitle(tr("新建通道"));
    
    if (dialog.exec() == QDialog::Accepted) {
        ChannelConfig config = dialog.getConfig();
        Channel* channel = m_channelManager->createChannel(config);
        if (channel) {
            m_configModified = true;
            statusBar()->showMessage(tr("通道 '%1' 已创建").arg(config.name), 3000);
            updateChannelTable();
        } else {
        QMessageBox::warning(
            this, tr("错误"),
                tr("创建通道失败：通道名称 '%1' 已存在").arg(config.name));
      }
        }
    }
}

void MainWindow::onLoadConfig() {
    if (!saveConfigIfNeeded()) {
        return;
    }
    
  QString filename =
      QFileDialog::getOpenFileName(this, tr("打开配置文件"), QString(),
        tr("JSON配置文件 (*.json);;所有文件 (*.*)"));
    
    if (!filename.isEmpty()) {
        if (m_channelManager->loadConfig(filename)) {
            m_currentConfigFile = filename;
            m_configModified = false;
            statusBar()->showMessage(tr("配置已加载: %1").arg(filename), 5000);
            updateChannelTable();
        } else {
            QMessageBox::critical(this, tr("错误"), 
                tr("加载配置文件失败: %1").arg(filename));
        }
    }
}

void MainWindow::onSaveConfig() {
    if (m_currentConfigFile.isEmpty()) {
        onSaveConfigAs();
        return;
    }
    
    saveConfigWithChannelRestart(m_currentConfigFile);
}

void MainWindow::onSaveConfigAs() {
  QString filename =
      QFileDialog::getSaveFileName(this, tr("保存配置文件"), "config.json",
        tr("JSON配置文件 (*.json);;所有文件 (*.*)"));
    
    if (!filename.isEmpty()) {
        saveConfigWithChannelRestart(filename);
    }
}

void MainWindow::saveConfigWithChannelRestart(const QString& filename) {
    // 收集所有运行中的通道
    QStringList runningChannels;
    QList<QString> allChannels = m_channelManager->getChannelNames();
    
    for (const QString& name : allChannels) {
        Channel* channel = m_channelManager->getChannel(name);
        if (channel && channel->getState() == ChannelState::Running) {
            runningChannels.append(name);
        }
    }
    
    // 如果有运行中的通道，先停止
    if (!runningChannels.isEmpty()) {
    qInfo() << "Stopping" << runningChannels.size()
            << "running channels before saving config...";
    statusBar()->showMessage(tr("正在停止 %1 个运行中的通道以保存配置...")
                                 .arg(runningChannels.size()),
                             3000);
        
        // 停止所有运行中的通道
        for (const QString& name : runningChannels) {
            Channel* channel = m_channelManager->getChannel(name);
            if (channel) {
                channel->stop();
            }
        }
        
        // 等待所有通道停止（最多等待5秒）
        int waitCount = 0;
        bool allStopped = false;
        while (waitCount < 50 && !allStopped) {
            QThread::msleep(100);
            QCoreApplication::processEvents();
            
            allStopped = true;
            for (const QString& name : runningChannels) {
                Channel* channel = m_channelManager->getChannel(name);
                if (channel && channel->getState() != ChannelState::Stopped) {
                    allStopped = false;
                    break;
                }
            }
            waitCount++;
        }
        
        if (!allStopped) {
            QMessageBox::warning(this, tr("警告"), 
                tr("部分通道停止超时，但将继续保存配置"));
        }
        
        updateChannelTable();
        qInfo() << "All channels stopped, proceeding to save config...";
    }
    
    // 保存配置
    bool saveSuccess = m_channelManager->saveConfig(filename);
    
    if (saveSuccess) {
        m_currentConfigFile = filename;
        m_configModified = false;
        statusBar()->showMessage(tr("配置已保存: %1").arg(filename), 3000);
        qInfo() << "Config saved successfully to:" << filename;
    } else {
        QMessageBox::critical(this, tr("错误"), 
            tr("保存配置文件失败: %1").arg(filename));
        
        // 保存失败，但仍然尝试重启通道
        if (!runningChannels.isEmpty()) {
            statusBar()->showMessage(tr("保存失败，正在重启通道..."), 3000);
        }
    }
    
    // 重新启动之前运行的通道
    if (!runningChannels.isEmpty()) {
        qInfo() << "Restarting" << runningChannels.size() << "channels...";
        statusBar()->showMessage(
            tr("正在重新启动 %1 个通道...").arg(runningChannels.size()), 3000);
        
        int successCount = 0;
        for (const QString& name : runningChannels) {
            Channel* channel = m_channelManager->getChannel(name);
            if (channel && channel->start()) {
                successCount++;
            }
        }
        
        updateChannelTable();
        
        if (saveSuccess) {
            if (successCount == runningChannels.size()) {
                statusBar()->showMessage(
                    tr("配置已保存，%1 个通道已重新启动").arg(successCount), 5000);
            } else {
                QMessageBox::warning(this, tr("警告"), 
                    tr("配置已保存，但只有 %1/%2 个通道成功重启")
                                 .arg(successCount)
                                 .arg(runningChannels.size()));
        statusBar()->showMessage(tr("配置已保存，部分通道启动失败 (%1/%2)")
                                     .arg(successCount)
                                     .arg(runningChannels.size()),
                                 5000);
            }
        } else {
      statusBar()->showMessage(tr("保存失败，%1/%2 个通道已重启")
                                   .arg(successCount)
                                   .arg(runningChannels.size()),
                               5000);
        }
        
    qInfo() << "Restart complete:" << successCount << "/"
            << runningChannels.size() << "channels running";
    }
}

void MainWindow::onExit() { close(); }

void MainWindow::onEditChannel() {
    QString channelName = getSelectedChannelName();
    if (channelName.isEmpty()) {
        return;
    }
    
    Channel* channel = m_channelManager->getChannel(channelName);
    if (!channel) {
        return;
    }
    
    // 记录通道原来的运行状态
    bool wasRunning = (channel->getState() == ChannelState::Running);
    
    ChannelConfigDialog dialog(channel, m_channelManager, this);
    dialog.setWindowTitle(tr("编辑通道 - %1").arg(channelName));
    
    if (dialog.exec() == QDialog::Accepted) {
        ChannelConfig config = dialog.getConfig();
        
        // 如果通道正在运行，需要先停止才能配置
        if (wasRunning) {
      qInfo() << "Stopping channel" << channelName
              << "before applying new configuration...";
            channel->stop();
            
            // 等待通道完全停止（最多等待3秒）
            int waitCount = 0;
            while (channel->getState() != ChannelState::Stopped && waitCount < 30) {
                QThread::msleep(100);
                QCoreApplication::processEvents();
                waitCount++;
            }
            
            if (channel->getState() != ChannelState::Stopped) {
        QMessageBox::warning(
            this, tr("警告"),
                    tr("通道 '%1' 停止超时，无法应用新配置").arg(channelName));
                return;
            }
            
            updateChannelTable();
        }
        
        // 应用新配置
        if (channel->configure(config)) {
            m_configModified = true;
            
            // 如果通道原来在运行，重新启动
            if (wasRunning) {
        qInfo() << "Restarting channel" << channelName
                << "with new configuration...";
                if (channel->start()) {
                    statusBar()->showMessage(
                        tr("通道 '%1' 配置已更新并重新启动").arg(channelName), 3000);
                } else {
          QMessageBox::warning(
              this, tr("警告"),
                        tr("通道 '%1' 配置已更新，但重新启动失败").arg(channelName));
                    statusBar()->showMessage(
                        tr("通道 '%1' 配置已更新（启动失败）").arg(channelName), 5000);
                }
            } else {
        statusBar()->showMessage(tr("通道 '%1' 配置已更新").arg(channelName),
                                 3000);
            }
            
            updateChannelTable();
        } else {
            QMessageBox::warning(this, tr("错误"), tr("更新通道配置失败"));
            
            // 如果配置失败但通道原来在运行，尝试恢复运行状态
            if (wasRunning) {
                channel->start();
                updateChannelTable();
            }
        }
    }
}

void MainWindow::onDeleteChannel() {
  if (m_isRemoteMode) {
    // 远程模式：删除远程通道
    if (!m_remoteClient || !m_remoteClient->isConnected()) {
      QMessageBox::warning(this, tr("未连接"), tr("请先连接到远程服务"));
      return;
    }

    QString channelName;
    if (m_dashboardWidget) {
      channelName = m_dashboardWidget->getRemoteChannelName();
    }

    if (channelName.isEmpty()) {
      QMessageBox::information(this, tr("提示"), tr("请先选择一个远程通道"));
      return;
    }

    QMessageBox::StandardButton reply =
        QMessageBox::question(this, tr("确认删除远程通道"),
                              tr("确定要删除远程通道 '%1' "
                                 "吗？\n\n此操作将删除远程服务器上的通道配置。")
                                  .arg(channelName),
                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
      m_remoteClient->deleteChannel(channelName);
      statusBar()->showMessage(tr("正在删除远程通道 '%1'...").arg(channelName),
                               3000);
      // 清除选中状态
      if (m_dashboardWidget) {
        m_dashboardWidget->setRemoteMode(true, "");
      }
      // 延迟刷新通道列表
      QTimer::singleShot(500, this, [this]() {
        if (m_remoteClient && m_remoteClient->isConnected()) {
          m_remoteClient->getChannels();
        }
      });
    }
  } else {
    // 本地模式：删除本地通道
    QString channelName = getSelectedChannelName();
    if (channelName.isEmpty()) {
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("确认删除"), tr("确定要删除通道 '%1' 吗？").arg(channelName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (m_channelManager->deleteChannel(channelName)) {
            m_configModified = true;
            statusBar()->showMessage(tr("通道 '%1' 已删除").arg(channelName), 3000);
            updateChannelTable();
        } else {
            QMessageBox::warning(this, tr("错误"), tr("删除通道失败"));
      }
        }
    }
}

void MainWindow::onStartChannel() {
    QString channelName = getSelectedChannelName();
    if (!channelName.isEmpty()) {
    statusBar()->showMessage(
        tr("正在后台启动通道 '%1'，请稍候...").arg(channelName), 5000);
        m_channelManager->startChannel(channelName);
    }
}

void MainWindow::onStopChannel() {
    QString channelName = getSelectedChannelName();
    if (!channelName.isEmpty()) {
        m_channelManager->stopChannel(channelName);
    }
}

void MainWindow::onStartAllChannels() {
    int count = m_channelManager->getChannelCount();
  statusBar()->showMessage(tr("正在后台启动 %1 个通道，请稍候...").arg(count),
                           5000);
    m_channelManager->startAll();
}

void MainWindow::onStopAllChannels() {
    m_channelManager->stopAll();
    statusBar()->showMessage(tr("正在停止所有通道..."), 3000);
}

void MainWindow::onAbout() {
    QMessageBox::about(this, tr("关于 Modbus PlexLink"),
        tr("<h2>Modbus PlexLink</h2>"
           "<p>版本 1.0</p>"
           "<p>数据采集与虚拟化服务系统</p>"
           "<p>支持 Modbus TCP 协议的数据采集、转换和虚拟化</p>"
           "<p>Copyright © 2025</p>"));
}

void MainWindow::onShowAppSettings() {
  AppSettingsDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    // 设置已保存，检查端口是否变化
    AppSettings& settings = AppSettings::instance();
    quint16 newHttpPort = settings.httpPort();
    quint16 newWsPort = settings.wsPort();

    if (newHttpPort != m_localHttpPort || newWsPort != m_localWsPort) {
      // 端口变化，提示用户重启
      m_localHttpPort = newHttpPort;
      m_localWsPort = newWsPort;

      QMessageBox::information(this, tr("端口已更新"),
                               tr("新端口设置：\n"
                                  "HTTP API: %1\n"
                                  "WebSocket: %2\n\n"
                                  "端口更改将在下次启动时生效。\n"
                                  "或者您可以手动重启API服务器。")
                                   .arg(newHttpPort)
                                   .arg(newWsPort));
    }

    // 更新自动启动设置
    m_autoStartChannels = settings.autoStartChannels();
  }
}

void MainWindow::onShowLogViewer() {
    if (!m_logViewer) {
        m_logViewer = new LogViewerWidget();
        m_logViewer->setWindowIcon(windowIcon());
        m_logViewer->hide();
    }

    m_logViewer->show();
    m_logViewer->raise();
    m_logViewer->activateWindow();
}

void MainWindow::onShowAlarmManager() {
    if (!m_alarmManager) {
        QMessageBox::warning(this, tr("报警管理"), tr("报警管理器尚未初始化。"));
        return;
    }

    if (!m_alarmWidget) {
        m_alarmWidget = new AlarmWidget(m_alarmManager, m_channelManager);
        m_alarmWidget->setWindowIcon(windowIcon());
           // 连接录波回放信号
    connect(m_alarmWidget, &AlarmWidget::requestPlaybackInRecorder, this,
            &MainWindow::onPlaybackAlarmRecording);
    }

    m_alarmWidget->show();
    m_alarmWidget->raise();
    m_alarmWidget->activateWindow();
}

void MainWindow::onPlaybackAlarmRecording(
    const QString& csvFilePath, const AlarmRecordingData& recordingData) {
    Q_UNUSED(csvFilePath);
    
    // 确保波形记录器已创建
    if (!m_waveformRecorder) {
        m_waveformRecorder = new ::WaveformRecorderWidget();
        m_waveformRecorder->setWindowTitle(tr("指定点位录波 - Modbus PlexLink"));
        m_waveformRecorder->setWindowIcon(windowIcon());
        m_waveformRecorder->resize(1200, 700);
    }
    
    // 将 AlarmRecordingData::DataPoint 转换为 WaveformRecorderWidget 需要的格式
    QVector<QPair<qint64, QMap<QString, double>>> playbackData;
    playbackData.reserve(recordingData.data.size());
    
    for (const auto& dp : recordingData.data) {
        playbackData.append(qMakePair(dp.timestamp, dp.values));
    }
    
    // 构建标题
  QString title =
      tr("告警录波回放 - %1 (%2)")
        .arg(recordingData.alarmEventId.left(8))
        .arg(recordingData.startTime.toString("yyyy-MM-dd HH:mm:ss"));
    
    // 加载数据到波形记录器
  if (m_waveformRecorder->loadFromAlarmRecording(recordingData.recordedTags,
                                                 playbackData, title)) {
        // 显示波形记录器窗口
        m_waveformRecorder->show();
        m_waveformRecorder->raise();
        m_waveformRecorder->activateWindow();
        
    qInfo() << "Loaded alarm recording for playback:"
            << recordingData.alarmEventId << "with" << playbackData.size()
            << "data points";
    } else {
        QMessageBox::warning(this, tr("回放失败"), 
            tr("无法加载录波数据。可能数据为空或格式不正确。"));
    }
}

void MainWindow::onShowWaveformRecorder() {
    if (!m_waveformRecorder) {
        m_waveformRecorder = new ::WaveformRecorderWidget();
        m_waveformRecorder->setWindowTitle(tr("指定点位录波 - Modbus PlexLink"));
        m_waveformRecorder->setWindowIcon(windowIcon());
        m_waveformRecorder->resize(1200, 700);
        
        // 设置当前通道的数据模型
        if (m_currentChannel) {
            m_waveformRecorder->setDataModel(m_currentChannel->getDataModel());
        }
    }
    
    // 更新数据模型（如果当前通道改变）
    if (m_currentChannel) {
        m_waveformRecorder->setDataModel(m_currentChannel->getDataModel());
    }
    
    m_waveformRecorder->show();
    m_waveformRecorder->raise();
    m_waveformRecorder->activateWindow();
}

void MainWindow::onAlarmTriggered(const AlarmEvent& event) {
    // 更新报警指示器
    updateAlarmIndicator();

    // 构建通知消息
    QString priorityStr;
    switch (event.priority) {
    case AlarmPriority::Low:
      priorityStr = tr("低");
      break;
    case AlarmPriority::Medium:
      priorityStr = tr("中");
      break;
    case AlarmPriority::High:
      priorityStr = tr("高");
      break;
    case AlarmPriority::Critical:
      priorityStr = tr("紧急");
      break;
    }

    QString title = QString("[%1] %2").arg(priorityStr, event.ruleName);
    QString message = QString("%1\n通道: %2 | 标签: %3\n当前值: %4")
        .arg(event.message)
        .arg(event.channelName)
        .arg(event.tagName)
        .arg(event.value.toString());

    // 系统托盘通知
    if (m_trayIcon && m_trayIcon->isVisible()) {
        QSystemTrayIcon::MessageIcon icon;
        switch (event.priority) {
            case AlarmPriority::Critical:
            case AlarmPriority::High:
                icon = QSystemTrayIcon::Critical;
                break;
            case AlarmPriority::Medium:
                icon = QSystemTrayIcon::Warning;
                break;
            default:
                icon = QSystemTrayIcon::Information;
                break;
        }
        m_trayIcon->showMessage(title, message, icon, 5000);
    }

    // 播放报警音效（仅针对高优先级和紧急报警）
  if (event.priority == AlarmPriority::High ||
      event.priority == AlarmPriority::Critical) {
        // 播放系统默认的警告音
        QApplication::beep();
    }

    // 在状态栏显示报警信息
    statusBar()->showMessage(QString(tr("报警触发: %1")).arg(title), 10000);

    // 记录到日志
    qWarning() << "Alarm triggered:" << title << "-" << message;
}

bool MainWindow::saveConfigIfNeeded() {
    if (!m_configModified) {
        return true;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("保存配置"), tr("配置已修改，是否保存？"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    
    if (reply == QMessageBox::Cancel) {
        return false;
    }
    
    if (reply == QMessageBox::Yes) {
        onSaveConfig();
    }
    
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // 检查是否有运行中的通道
    int runningCount = 0;
    for (const QString& name : m_channelManager->getChannelNames()) {
        Channel* ch = m_channelManager->getChannel(name);
        if (ch && ch->getState() == ChannelState::Running) {
            runningCount++;
        }
    }
    
    // 如果有托盘图标且有运行中的通道，显示确认对话框
    if (m_trayIcon && m_trayIcon->isVisible()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("关闭确认"));
        msgBox.setIcon(QMessageBox::Question);
        
        if (runningCount > 0) {
            msgBox.setText(tr("当前有 %1 个通道正在运行。").arg(runningCount));
            msgBox.setInformativeText(tr("请选择操作："));
        } else {
            msgBox.setText(tr("确定要关闭程序吗？"));
            msgBox.setInformativeText(tr("请选择操作："));
        }
        
    QPushButton* minimizeBtn =
        msgBox.addButton(tr("最小化到托盘"), QMessageBox::ActionRole);
    QPushButton* exitBtn =
        msgBox.addButton(tr("完全退出"), QMessageBox::DestructiveRole);
    QPushButton* cancelBtn =
        msgBox.addButton(tr("取消"), QMessageBox::RejectRole);
        
        msgBox.setDefaultButton(cancelBtn);
        msgBox.exec();
        
        if (msgBox.clickedButton() == minimizeBtn) {
            // 最小化到托盘
        hide();
        showTrayMessage(tr("Modbus PlexLink"), 
            tr("程序已最小化到系统托盘，继续在后台运行"));
            event->ignore();
            return;
        } else if (msgBox.clickedButton() == exitBtn) {
            // 完全退出
            if (!saveConfigIfNeeded()) {
        event->ignore();
        return;
    }
            m_channelManager->stopAll();
            event->accept();
            return;
        } else {
            // 取消
            event->ignore();
            return;
        }
    }
    
    // 没有托盘图标时的默认行为
    if (!saveConfigIfNeeded()) {
        event->ignore();
        return;
    }
    
    // 停止所有通道
    m_channelManager->stopAll();
    
    event->accept();
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        // 最小化时正常最小化到任务栏，不自动隐藏到托盘
        // 用户可以通过关闭按钮选择是否最小化到托盘
        if (isMinimized()) {
            // 正常最小化行为，不做特殊处理
        }
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_alarmIndicator &&
      event->type() == QEvent::MouseButtonRelease) {
        onShowAlarmManager();
        return true;
    }

  // 处理远程通道卡片点击
  if (event->type() == QEvent::MouseButtonRelease) {
    QFrame* card = qobject_cast<QFrame*>(watched);
    if (card && card->objectName() == "remoteChannelCard") {
      QString channelName = card->property("channelName").toString();
      if (!channelName.isEmpty()) {
        // 选中远程通道
        qInfo() << "Selected remote channel:" << channelName;

        // 更新仪表盘显示
        if (m_dashboardWidget) {
          m_dashboardWidget->setRemoteMode(true, channelName);

          // 如果有缓存数据，立即显示
          if (m_remoteDataCache.contains(channelName)) {
            m_dashboardWidget->updateRemoteData(channelName,
                                                m_remoteDataCache[channelName]);
          }
        }

        // 更新报文面板显示远程通道名称
        if (m_messageLogPanel) {
          m_messageLogPanel->setRemoteMode(true, channelName);
        }

        // 请求该通道的数据
        if (m_remoteClient && m_remoteClient->isConnected()) {
          m_remoteClient->getData(channelName);
        }

        statusBar()->showMessage(tr("已选择远程通道: %1").arg(channelName),
                                 3000);
        return true;
      }
    }
  }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::createSystemTray() {
    // 创建托盘图标
    m_trayIcon = new QSystemTrayIcon(this);
    // 使用窗口图标作为托盘图标
    QIcon trayIcon = windowIcon();
    if (trayIcon.isNull()) {
        // 如果没有窗口图标，创建一个简单的默认图标
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(34, 139, 34));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(4, 4, 24, 24);
        painter.setPen(QPen(Qt::white, 2));
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(pixmap.rect(), Qt::AlignCenter, "M");
        trayIcon = QIcon(pixmap);
    }
    m_trayIcon->setIcon(trayIcon);
    m_trayIcon->setToolTip(tr("Modbus PlexLink - 数据采集与虚拟化服务系统"));
    
    // 创建托盘菜单
    m_trayMenu = new QMenu(this);
    
    m_showHideAction = new QAction(tr("显示主窗口"), this);
    connect(m_showHideAction, &QAction::triggered, this, &MainWindow::onShowHide);
    m_trayMenu->addAction(m_showHideAction);
    
    m_trayMenu->addSeparator();
    
    // 添加启动/停止所有通道选项
    m_trayMenu->addAction(m_startAllAction);
    m_trayMenu->addAction(m_stopAllAction);
    
    m_trayMenu->addSeparator();
    
    m_trayExitAction = new QAction(tr("退出程序"), this);
    connect(m_trayExitAction, &QAction::triggered, this, &MainWindow::onTrayExit);
    m_trayMenu->addAction(m_trayExitAction);
    
    m_trayIcon->setContextMenu(m_trayMenu);
    
    // 连接双击事件
  connect(m_trayIcon, &QSystemTrayIcon::activated, this,
          &MainWindow::onTrayIconActivated);
    
    // 显示托盘图标
    m_trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        onShowHide();
    }
}

void MainWindow::onShowHide() {
    if (isVisible() && !isMinimized()) {
        hide();
        m_showHideAction->setText(tr("显示主窗口"));
    showTrayMessage(tr("Modbus PlexLink"), tr("程序已最小化到系统托盘"));
    } else {
        show();
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        raise();
        activateWindow();
        m_showHideAction->setText(tr("隐藏主窗口"));
    }
}

void MainWindow::onTrayExit() {
    // 真正退出程序
    m_trayIcon->hide();
    
    if (!saveConfigIfNeeded()) {
        return;
    }
    
    // 停止所有通道
    m_channelManager->stopAll();
    
    QApplication::quit();
}

void MainWindow::showTrayMessage(const QString& title, const QString& message) {
    if (m_trayIcon && m_trayIcon->isVisible() && m_trayIcon->supportsMessages()) {
        m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
    }
}

void MainWindow::autoStartAllChannels() {
    int count = m_channelManager->getChannelCount();
    if (count > 0) {
        qInfo() << "Auto-starting all channels on startup:" << count << "channels";
        m_channelManager->startAll();
    statusBar()->showMessage(tr("自动启动 %1 个通道...").arg(count), 5000);
        showTrayMessage(tr("Modbus PlexLink"), 
            tr("已自动启动 %1 个通道").arg(count));
    }
}

void MainWindow::applyModernTheme() {
    // 全局样式表 - 现代浅色主题
    QString modernStyle = R"(
        /* 主窗口背景 */
        QMainWindow {
            background-color: #F9FAFB;
        }

        /* 左侧面板 */
        #leftPanel {
            background-color: #F3F4F6;
            border-right: 1px solid #E5E7EB;
        }

        /* 右侧面板 */
        #dashboardPanel {
            background-color: white;
            border: 1px solid #E5E7EB;
            border-radius: 8px;
        }

        #panelTitle {
            color: #111827;
            padding: 4px 0px;
        }

        /* 分割器样式 */
        QSplitter::handle {
            background-color: #E5E7EB;
        }

        QSplitter::handle:horizontal {
            width: 1px;
        }

        QSplitter::handle:vertical {
            height: 1px;
        }

        QSplitter::handle:hover {
            background-color: #3B82F6;
        }

        /* 滚动条样式 */
        QScrollArea {
            border: none;
            background-color: transparent;
        }

        QScrollBar:vertical {
            border: none;
            background: #F3F4F6;
            width: 10px;
            margin: 0px;
        }

        QScrollBar::handle:vertical {
            background: #D1D5DB;
            min-height: 20px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical:hover {
            background: #9CA3AF;
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
        }

        /* 头部图标按钮 */
        #iconButtonHeader {
            background-color: #3B82F6;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 14pt;
        }

        #iconButtonHeader:hover {
            background-color: #2563EB;
        }

        #iconButtonHeader:pressed {
            background-color: #1D4ED8;
        }

        /* 工具栏样式 */
        QToolBar {
            background-color: white;
            border-bottom: 1px solid #E5E7EB;
            spacing: 4px;
            padding: 4px;
        }

        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 6px;
            color: #374151;
        }

        QToolButton:hover {
            background-color: #F3F4F6;
            border-color: #E5E7EB;
        }

        QToolButton:pressed {
            background-color: #E5E7EB;
        }

        /* 状态栏样式 */
        QStatusBar {
            background-color: white;
            border-top: 1px solid #E5E7EB;
            color: #6B7280;
        }

        QStatusBar::item {
            border: none;
        }
    )";

    setStyleSheet(modernStyle);
}

// ============================================================================
// 通道卡片管理
// ============================================================================

void MainWindow::updateChannelCards() {
    // 清除现有卡片
    qDeleteAll(m_channelCards);
    m_channelCards.clear();

    // 清空布局
    QLayoutItem* item;
    while ((item = m_channelListLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // 获取所有通道
    QList<Channel*> channels = m_channelManager->getAllChannels();

    if (channels.isEmpty()) {
        // 显示空状态
    QLabel* emptyLabel =
        new QLabel(tr("暂无通道\n\n点击 ➕ 创建新通道"), m_channelListWidget);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(R"(
            QLabel {
                color: #9CA3AF;
                font-size: 12px;
                padding: 40px 20px;
            }
        )");
        m_channelListLayout->addWidget(emptyLabel);
    } else {
        // 创建通道卡片
        for (Channel* channel : channels) {
      ChannelCardWidget* card =
          new ChannelCardWidget(channel, m_channelListWidget);

            // 连接信号
      connect(card, &ChannelCardWidget::clicked, this,
              &MainWindow::selectChannel);
      connect(card, &ChannelCardWidget::startRequested, this,
              [this](Channel* ch) {
                        if (ch) {
                            ch->start();
                        }
                    });
      connect(card, &ChannelCardWidget::stopRequested, this,
              [this](Channel* ch) {
                        if (ch) {
                            ch->stop();
                        }
                    });
      connect(card, &ChannelCardWidget::editRequested, this,
              [this](Channel* ch) {
                        if (ch) {
                            m_currentChannel = ch;
                            onEditChannel();
                        }
                    });
      connect(card, &ChannelCardWidget::deleteRequested, this,
              [this](Channel* ch) {
                        if (ch) {
                            m_currentChannel = ch;
                            onDeleteChannel();
                        }
                    });

            m_channelCards.append(card);
            m_channelListLayout->addWidget(card);
        }
    }

    // 添加弹簧
    m_channelListLayout->addStretch();

    // 如果有通道且没有选中，选中第一个
    if (!channels.isEmpty() && !m_currentChannel) {
        selectChannel(channels.first());
    }
}

void MainWindow::selectChannel(Channel* channel) {
    if (m_currentChannel == channel) {
        return;  // 已经选中
    }

    // 断开旧通道的信号连接
    if (m_currentChannel) {
        disconnect(m_currentChannel, nullptr, this, nullptr);
        if (m_messageLogPanel && m_messageLogPanel->getMessageViewer()) {
      disconnect(m_currentChannel, nullptr,
                 m_messageLogPanel->getMessageViewer(), nullptr);
        }
    }

    // 更新选中状态
    for (ChannelCardWidget* card : m_channelCards) {
        card->setSelected(card->getChannel() == channel);
    }

    // 断开旧通道的连接
    if (m_currentChannel) {
        disconnect(m_currentChannel, &Channel::modbusMessage, this, nullptr);
    }

    m_currentChannel = channel;

    // 更新右侧数据面板
    if (m_dashboardWidget) {
        m_dashboardWidget->setChannel(channel);
    }

    // 更新报文日志面板
    if (m_messageLogPanel) {
        m_messageLogPanel->setChannel(channel);

        // 连接新通道的Modbus报文信号
        if (channel && m_messageLogPanel->getMessageViewer()) {
      connect(
          channel, &Channel::modbusMessage, this,
          [this](const QString& source, const QString& direction,
                                const QString& device, const QString& function,
                                const QString& address, const QString& data, bool success) {
                        if (m_messageLogPanel && m_messageLogPanel->getMessageViewer()) {
                            m_messageLogPanel->getMessageViewer()->addMessage(
                  direction, source + ":" + device, function, address, data,
                  success);
                        }
                    });
        }
    }

    // 更新状态栏
    if (channel) {
    statusBar()->showMessage(tr("已选中通道: %1").arg(channel->getName()),
                             3000);
    }
}

Channel* MainWindow::getSelectedChannel() const { return m_currentChannel; }

// ============================================================================
// 更新 onChannelCreated/Deleted/StateChanged 方法
// ============================================================================

void MainWindow::onChannelCreated(const QString& name) {
    qInfo() << "Channel created:" << name;
    updateChannelCards();  // 重新生成卡片
    updateStatusBar();
    updateActions();
    m_configModified = true;
}

void MainWindow::onChannelDeleted(const QString& name) {
    qInfo() << "Channel deleted:" << name;

  // 如果删除的是当前选中的通道，清理右侧面板
    if (m_currentChannel && m_currentChannel->getName() == name) {
    // 先断开信号连接
    disconnect(m_currentChannel, nullptr, this, nullptr);
    if (m_messageLogPanel && m_messageLogPanel->getMessageViewer()) {
      disconnect(m_currentChannel, nullptr,
                 m_messageLogPanel->getMessageViewer(), nullptr);
    }

        m_currentChannel = nullptr;

    // 清理右侧数据面板
    if (m_dashboardWidget) {
      m_dashboardWidget->setChannel(nullptr);
    }
    if (m_messageLogPanel) {
      m_messageLogPanel->setChannel(nullptr);
    }
    }

  updateChannelCards();  // 重新生成卡片（会自动选中第一个可用通道）
    updateStatusBar();
    updateActions();
    m_configModified = true;
}

void MainWindow::onChannelStateChanged(const QString& name,
                                       ChannelState state) {
    // 找到对应的卡片并更新
    for (ChannelCardWidget* card : m_channelCards) {
        if (card->getChannel() && card->getChannel()->getName() == name) {
            card->updateDisplay();
            break;
        }
    }

    updateStatusBar();

    // 如果是当前选中的通道，更新数据面板
    if (m_currentChannel && m_currentChannel->getName() == name) {
        if (state == ChannelState::Running) {
            // 刷新数据显示
            if (m_dashboardWidget) {
                m_dashboardWidget->refreshDisplay();
            }
        }
    }
}

// ============================================================================
// 新的 updateChannelTable 实现（适配卡片列表）
// ============================================================================

void MainWindow::updateChannelTable() {
  // 本地模式：更新所有卡片的显示
  if (!m_isRemoteMode) {
    for (ChannelCardWidget* card : m_channelCards) {
        card->updateDisplay();
    }
  } else {
    // 远程模式：定期刷新当前选中通道的数据
    if (m_remoteClient && m_remoteClient->isConnected() && m_dashboardWidget) {
      QString currentRemoteChannel = m_dashboardWidget->getRemoteChannelName();
      if (!currentRemoteChannel.isEmpty()) {
        m_remoteClient->getData(currentRemoteChannel);
      }
    }
    }
}

// ============================================================================
// 获取选中通道名称（兼容旧代码）
// ============================================================================

QString MainWindow::getSelectedChannelName() const {
    if (m_currentChannel) {
        return m_currentChannel->getName();
    }
    return QString();
}

// ============================================================================
// 远程连接功能
// ============================================================================

void MainWindow::onConnectRemote() {
    RemoteConnectionDialog dialog(this);
    
    // 传递当前连接状态给对话框
    bool isCurrentlyConnected = m_remoteClient && m_remoteClient->isConnected();
    if (!m_remoteHost.isEmpty()) {
        dialog.setCurrentConnection(m_remoteHost, isCurrentlyConnected);
    }
    
    if (dialog.exec() == QDialog::Accepted) {
        QString host = dialog.getHost();
        quint16 httpPort = dialog.getHttpPort();
        quint16 wsPort = dialog.getWsPort();
        
        // 如果已有远程客户端，先清理
        if (m_remoteClient) {
            // 断开所有信号连接，防止重复连接
            disconnect(m_remoteClient, nullptr, this, nullptr);
            m_remoteClient->disconnect();
            m_remoteClient->deleteLater();
            m_remoteClient = nullptr;
        }
        
        // 创建新的远程客户端
        m_remoteClient = new RemoteClient(this);
        
        // 连接信号
    connect(m_remoteClient, &RemoteClient::connected, this,
            &MainWindow::onRemoteConnected);
    connect(m_remoteClient, &RemoteClient::disconnected, this,
            &MainWindow::onRemoteDisconnected);
    connect(m_remoteClient, &RemoteClient::connectionError, this,
            &MainWindow::onRemoteError);
    connect(m_remoteClient, &RemoteClient::channelsReceived, this,
            &MainWindow::onRemoteChannelsReceived);
    connect(m_remoteClient, &RemoteClient::realtimeDataReceived, this,
            &MainWindow::onRemoteDataReceived);

    // 连接HTTP API数据响应（用于点击通道后获取数据）
    connect(m_remoteClient, &RemoteClient::dataReceived, this,
            [this](const QString& channelName, const QJsonObject& data) {
              if (m_dashboardWidget && m_isRemoteMode) {
                m_dashboardWidget->updateRemoteData(channelName, data);
              }
              m_remoteDataCache[channelName] = data;
            });

    // 连接远程通道启动/停止响应
    connect(m_remoteClient, &RemoteClient::channelStarted, this,
            [this](const QString& name, bool success) {
              if (success) {
                statusBar()->showMessage(tr("远程通道 '%1' 已启动").arg(name),
                                         3000);
              } else {
                statusBar()->showMessage(tr("启动远程通道 '%1' 失败").arg(name),
                                         5000);
              }
              // 刷新通道列表
              m_remoteClient->getChannels();
            });

    connect(m_remoteClient, &RemoteClient::channelStopped, this,
            [this](const QString& name, bool success) {
              if (success) {
                statusBar()->showMessage(tr("远程通道 '%1' 已停止").arg(name),
                                         3000);
              } else {
                statusBar()->showMessage(tr("停止远程通道 '%1' 失败").arg(name),
                                         5000);
              }
              // 刷新通道列表
              m_remoteClient->getChannels();
            });

    // 连接远程通道创建/删除响应
    connect(m_remoteClient, &RemoteClient::channelCreated, this,
            [this](const QString& name, bool success) {
              if (success) {
                statusBar()->showMessage(tr("远程通道 '%1' 已创建").arg(name),
                                         3000);
              } else {
                statusBar()->showMessage(tr("创建远程通道 '%1' 失败").arg(name),
                                         5000);
              }
              // 刷新通道列表
              m_remoteClient->getChannels();
            });

    connect(m_remoteClient, &RemoteClient::channelDeleted, this,
            [this](const QString& name, bool success) {
              if (success) {
                statusBar()->showMessage(tr("远程通道 '%1' 已删除").arg(name),
                                         3000);
                // 如果删除的是当前选中的通道，清除选中状态
                if (m_dashboardWidget &&
                    m_dashboardWidget->getRemoteChannelName() == name) {
                  m_dashboardWidget->setRemoteMode(true, "");
                }
              } else {
                statusBar()->showMessage(tr("删除远程通道 '%1' 失败").arg(name),
                                         5000);
              }
              // 刷新通道列表
              m_remoteClient->getChannels();
            });

    // 连接远程通道配置响应（用于编辑对话框）
    connect(m_remoteClient, &RemoteClient::channelReceived, this,
            [this](const QJsonObject& channelData) {
              QString name = channelData["name"].toString();
              // 检查是否是等待编辑的通道
              if (!m_pendingEditRemoteChannel.isEmpty() && 
                  m_pendingEditRemoteChannel == name) {
                m_pendingEditRemoteChannel.clear();
                
                // 将远程配置转换为 ChannelConfig
                ChannelConfig config;
                config.name = name;
                config.enabled = channelData["enabled"].toBool(true);
                config.description = channelData["description"].toString();
                
                // 采集器配置
                QJsonArray collectorsArray = channelData["collectors"].toArray();
                for (const QJsonValue& val : collectorsArray) {
                  config.collectors.append(val.toObject());
                }
                
                // 服务器配置
                QJsonArray serversArray = channelData["servers"].toArray();
                for (const QJsonValue& val : serversArray) {
                  config.servers.append(val.toObject());
                }
                
                // 打开编辑对话框（使用本地 ChannelManager 作为参考，但不会实际修改）
                ChannelConfigDialog dialog(nullptr, m_channelManager, this);
                dialog.setWindowTitle(tr("编辑远程通道 - %1").arg(name));
                dialog.setConfig(config);  // 设置远程配置
                
                if (dialog.exec() == QDialog::Accepted) {
                  ChannelConfig newConfig = dialog.getConfig();
                  
                  // 将配置转换为 JSON 并更新远程通道
                  QJsonObject configJson;
                  configJson["name"] = newConfig.name;
                  configJson["enabled"] = newConfig.enabled;
                  configJson["description"] = newConfig.description;
                  
                  QJsonArray newCollectorsArray;
                  for (const QJsonObject& collector : newConfig.collectors) {
                    newCollectorsArray.append(collector);
                  }
                  configJson["collectors"] = newCollectorsArray;
                  
                  QJsonArray newServersArray;
                  for (const QJsonObject& server : newConfig.servers) {
                    newServersArray.append(server);
                  }
                  configJson["servers"] = newServersArray;
                  
                  // 通过 API 更新远程通道
                  m_remoteClient->updateChannel(name, configJson);
                  statusBar()->showMessage(tr("正在更新远程通道 '%1'...").arg(name), 3000);
                }
              }
            });

    // 连接远程通道更新响应
    connect(m_remoteClient, &RemoteClient::channelUpdated, this,
            [this](const QString& name, bool success) {
              if (success) {
                statusBar()->showMessage(tr("远程通道 '%1' 配置已更新").arg(name), 3000);
              } else {
                statusBar()->showMessage(tr("更新远程通道 '%1' 失败").arg(name), 5000);
              }
              // 刷新通道列表
              m_remoteClient->getChannels();
            });
        
        // 设置认证
        if (dialog.useAuthentication()) {
      m_remoteClient->setCredentials(dialog.getUsername(),
                                     dialog.getPassword());
        }
        
        m_remoteHost = QString("%1:%2").arg(host).arg(httpPort);
        
        statusBar()->showMessage(tr("正在连接到 %1...").arg(m_remoteHost));
    qInfo() << "Connecting to remote service:" << host << "HTTP:" << httpPort
            << "WS:" << wsPort;
        
        m_remoteClient->connectToHost(host, httpPort, wsPort);
    }
}

void MainWindow::onDisconnectRemote() {
  // 正确清理远程客户端
    if (m_remoteClient) {
    disconnect(m_remoteClient, nullptr, this, nullptr);  // 断开所有信号
        m_remoteClient->disconnect();
    m_remoteClient->deleteLater();
    m_remoteClient = nullptr;
  }

  // 退出远程模式
  exitRemoteMode();

  // 更新模式指示器
  if (m_modeIndicator) {
    m_modeIndicator->setLocalMode(m_localApiServer != nullptr, m_localHttpPort,
                                  m_localWsPort);
  }

    m_connectRemoteAction->setEnabled(true);
  m_connectRemoteAction->setText(tr("🌐 远程连接"));
    m_disconnectRemoteAction->setEnabled(false);
    m_switchToLocalAction->setEnabled(false);
    
    if (m_connectionStatusLabel) {
    m_connectionStatusLabel->setText(tr("本地模式"));
    m_connectionStatusLabel->setStyleSheet("color: #6B7280;");
    }
    
  setWindowTitle("Modbus PlexLink - 数据采集与虚拟化服务系统");
  statusBar()->showMessage(tr("已断开远程连接，切换回本地模式"), 5000);
    
  qInfo() << "Disconnected from remote service, switched to local mode";
}

void MainWindow::onSwitchToLocalMode() {
    // 如果还有远程连接，先断开
    if (m_remoteClient) {
        disconnect(m_remoteClient, nullptr, this, nullptr);
        m_remoteClient->disconnect();
        m_remoteClient->deleteLater();
        m_remoteClient = nullptr;
    }
    
  // 先将 m_currentChannel 设为 nullptr，避免 exitRemoteMode 中的 selectChannel 重复连接信号
  if (m_currentChannel) {
    disconnect(m_currentChannel, nullptr, this, nullptr);
    if (m_messageLogPanel && m_messageLogPanel->getMessageViewer()) {
      disconnect(m_currentChannel, nullptr, m_messageLogPanel->getMessageViewer(), nullptr);
    }
    m_currentChannel = nullptr;
  }
  
  // 退出远程模式（注意：exitRemoteMode 内部会调用 selectChannel）
    m_remoteHost.clear();
  m_isRemoteMode = false;
  m_remoteChannelsCache = QJsonArray();
  m_remoteDataCache.clear();

  // 根据之前的设置决定是否启动API服务器
  initializeMode(ApplicationMode::LocalWithApi);
    
    // 更新按钮状态
    m_connectRemoteAction->setEnabled(true);
  m_connectRemoteAction->setText(tr("🌐 远程连接"));
    m_disconnectRemoteAction->setEnabled(false);
    m_switchToLocalAction->setEnabled(false);
    
    // 更新状态显示
    if (m_connectionStatusLabel) {
        m_connectionStatusLabel->setText(tr("本地模式"));
        m_connectionStatusLabel->setStyleSheet("color: #6B7280;");
    }
    
  // 恢复仪表盘为本地模式
  if (m_dashboardWidget) {
    m_dashboardWidget->setRemoteMode(false);
  }

  // 恢复报文面板为本地模式（清除远程报文）
  if (m_messageLogPanel) {
    m_messageLogPanel->setRemoteMode(false);
  }
    
    // 刷新本地通道列表显示
    updateChannelCards();
    
    // 如果有本地通道，选中第一个
    QList<Channel*> channels = m_channelManager->getAllChannels();
    if (!channels.isEmpty()) {
        selectChannel(channels.first());

    // 询问用户是否要重新启动本地通道
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("启动本地通道"),
        tr("已切换回本地模式，共 %1 个本地通道。\n\n是否启动所有本地通道？")
            .arg(channels.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (reply == QMessageBox::Yes) {
      m_channelManager->startAll();
      statusBar()->showMessage(tr("已切换到本地模式，正在启动 %1 个本地通道...")
                                   .arg(channels.size()),
                               5000);
    } else {
      statusBar()->showMessage(tr("已切换到本地模式，本地通道未启动（点击'全部"
                                  "启动'按钮可手动启动）"),
                               5000);
    }
  } else {
        if (m_dashboardWidget) {
            m_dashboardWidget->setChannel(nullptr);
        }
        if (m_messageLogPanel) {
            m_messageLogPanel->setChannel(nullptr);
        }
    statusBar()->showMessage(tr("已切换到本地模式，暂无本地通道"), 5000);
    }
    
    updateStatusBar();
  qInfo() << "Switched to local mode with" << channels.size()
          << "local channels";
}

void MainWindow::onRemoteConnected() {
  // 进入远程模式
  enterRemoteMode();

  // 更新模式指示器
  if (m_modeIndicator) {
    m_modeIndicator->setRemoteMode(m_remoteHost, true);
  }
    
    // 更新按钮状态
  m_connectRemoteAction->setEnabled(true);
  m_connectRemoteAction->setText(tr("🌐 切换远程"));
    m_disconnectRemoteAction->setEnabled(true);
  m_switchToLocalAction->setEnabled(true);
    
    setWindowTitle(QString("Modbus PlexLink - 远程: %1").arg(m_remoteHost));
    
    if (m_connectionStatusLabel) {
        m_connectionStatusLabel->setText(tr("🟢 远程: %1").arg(m_remoteHost));
    m_connectionStatusLabel->setStyleSheet(
        "color: #10B981; font-weight: bold;");
  }
  
  // 连接远程通道状态变化和报文信号
  if (m_remoteClient && m_messageLogPanel && m_messageLogPanel->getMessageViewer()) {
    // 远程通道状态变化
    connect(m_remoteClient, &RemoteClient::realtimeChannelEventReceived, this,
            [this](const QJsonObject& event) {
              QString channelName = event["channel"].toString();
              QString action = event["action"].toString();
              
              if (action == "stateChanged") {
                QString stateString = event["stateString"].toString();
                bool running = event["running"].toBool();
                
                QString statusMsg;
                QString color;
                
                if (running) {
                  statusMsg = tr("🟢 [远程] 通道 '%1' 已启动，开始采集数据").arg(channelName);
                  color = "#10B981";
                } else {
                  statusMsg = tr("⚫ [远程] 通道 '%1' 已停止").arg(channelName);
                  color = "#6B7280";
                }
                
                // 如果当前选中的是此远程通道，显示状态消息
                if (m_isRemoteMode && m_dashboardWidget && 
                    m_dashboardWidget->getRemoteChannelName() == channelName) {
                  m_messageLogPanel->getMessageViewer()->addStatusMessage(statusMsg, color);
                }
              } else if (action == "created") {
                QString statusMsg = tr("📡 [远程] 通道 '%1' 已创建").arg(channelName);
                m_messageLogPanel->getMessageViewer()->addStatusMessage(statusMsg, "#3B82F6");
              } else if (action == "deleted") {
                QString statusMsg = tr("🗑️ [远程] 通道 '%1' 已删除").arg(channelName);
                m_messageLogPanel->getMessageViewer()->addStatusMessage(statusMsg, "#EF4444");
              }
            });
    
    // 远程Modbus报文
    connect(m_remoteClient, &RemoteClient::realtimeMessageReceived, this,
            [this](const QJsonObject& message) {
              QString channelName = message["channel"].toString();
              
              // 如果当前选中的是此远程通道，显示报文
              if (m_isRemoteMode && m_dashboardWidget && 
                  m_dashboardWidget->getRemoteChannelName() == channelName &&
                  m_messageLogPanel && m_messageLogPanel->getMessageViewer()) {
                QString source = message["source"].toString();
                QString direction = message["direction"].toString();
                QString device = message["device"].toString();
                QString function = message["function"].toString();
                QString address = message["address"].toString();
                QString data = message["data"].toString();
                bool success = message["success"].toBool();
                
                // 添加远程标记
                m_messageLogPanel->getMessageViewer()->addMessage(
                    direction, "[远程]" + source + ":" + device, function, address, data, success);
              }
            });
    }
    
    statusBar()->showMessage(tr("已连接到远程服务 %1").arg(m_remoteHost), 5000);
    qInfo() << "Connected to remote service:" << m_remoteHost;
    
    // 订阅实时数据
    m_remoteClient->subscribeToData();
    m_remoteClient->subscribeToStatus();
  m_remoteClient->subscribeToAlarms();
  
  // 订阅远程通道状态和报文
  m_remoteClient->subscribeToChannel();
  m_remoteClient->subscribeToMessage();
    
    // 获取远程通道列表
    m_remoteClient->getChannels();
}

void MainWindow::onRemoteDisconnected() {
    if (m_isRemoteMode) {
    // 更新模式指示器（保持远程模式但显示断开状态）
    if (m_modeIndicator) {
      m_modeIndicator->setRemoteMode(m_remoteHost, false);
    }
        
        // 更新按钮状态
        m_connectRemoteAction->setEnabled(true);
    m_connectRemoteAction->setText(tr("🌐 远程连接"));
        m_disconnectRemoteAction->setEnabled(false);
    m_switchToLocalAction->setEnabled(true);
        
        if (m_connectionStatusLabel) {
            m_connectionStatusLabel->setText(tr("🔴 已断开"));
      m_connectionStatusLabel->setStyleSheet(
          "color: #EF4444; font-weight: bold;");
        }
        
    setWindowTitle("Modbus PlexLink - 远程连接已断开");
    statusBar()->showMessage(
        tr("与远程服务 %1 的连接已断开。点击模式指示器切换回本地模式或重新连接")
            .arg(m_remoteHost),
        10000);
        
        qWarning() << "Lost connection to remote service:" << m_remoteHost;
    }
}

void MainWindow::onRemoteError(const QString& error) {
  QMessageBox::warning(
      this, tr("远程连接错误"),
      tr("连接远程服务失败:\n%1\n\n点击模式指示器可重新连接或切换到本地模式。")
          .arg(error));

  // 更新模式指示器
  if (m_modeIndicator) {
    m_modeIndicator->setRemoteMode(m_remoteHost, false);
  }
    
    if (m_connectionStatusLabel) {
        m_connectionStatusLabel->setText(tr("⚠️ 连接错误"));
    m_connectionStatusLabel->setStyleSheet(
        "color: #EF4444; font-weight: bold;");
    }
    
    // 更新按钮状态
    m_connectRemoteAction->setEnabled(true);
  m_connectRemoteAction->setText(tr("🌐 远程连接"));
    m_disconnectRemoteAction->setEnabled(false);
  m_switchToLocalAction->setEnabled(true);
    
  setWindowTitle("Modbus PlexLink - 连接失败");
    
    qWarning() << "Remote connection error:" << error;
}

void MainWindow::onRemoteChannelsReceived(const QJsonArray& channels) {
    qInfo() << "Received" << channels.size() << "channels from remote service";

  // 获取旧的通道名称集合，用于检测删除的通道
  QSet<QString> oldChannelNames;
  for (const QJsonValue& val : m_remoteChannelsCache) {
    oldChannelNames.insert(val.toObject()["name"].toString());
  }

  // 获取新的通道名称集合
  QSet<QString> newChannelNames;
  for (const QJsonValue& val : channels) {
    newChannelNames.insert(val.toObject()["name"].toString());
  }

  // 清理已删除通道的缓存数据
  QSet<QString> deletedChannels = oldChannelNames - newChannelNames;
  for (const QString& deletedName : deletedChannels) {
    m_remoteDataCache.remove(deletedName);
    qInfo() << "Remote channel deleted:" << deletedName;
  }

  // 缓存远程通道数据
  m_remoteChannelsCache = channels;
    
    // 显示远程通道信息
    for (const QJsonValue& val : channels) {
        QJsonObject ch = val.toObject();
        qInfo() << "  Remote channel:" << ch["name"].toString() 
                << "running:" << ch["running"].toBool()
                << "collectors:" << ch["collectorCount"].toInt();
    }

  // ★ 更新远程通道卡片列表 ★
  updateRemoteChannelCards();

  // 更新仪表盘 - 检查当前选中的远程通道是否仍然存在
  if (m_dashboardWidget) {
    QString currentRemoteChannel = m_dashboardWidget->getRemoteChannelName();
    if (!currentRemoteChannel.isEmpty() &&
        !newChannelNames.contains(currentRemoteChannel)) {
      // 当前选中的通道已被删除，清空显示
      m_dashboardWidget->setRemoteMode(true, "");  // 重置为无选中状态
      statusBar()->showMessage(
          tr("远程通道 '%1' 已被删除").arg(currentRemoteChannel), 5000);
    }
  }
    
    // 更新状态栏
  int runningCount = 0;
  for (const QJsonValue& val : channels) {
    if (val.toObject()["running"].toBool()) {
      runningCount++;
    }
  }

  m_channelCountLabel->setText(tr("通道: %1").arg(channels.size()));
  m_runningCountLabel->setText(tr("运行: %1").arg(runningCount));
  statusBar()->showMessage(tr("远程服务: %1 个通道，%2 个运行中")
                               .arg(channels.size())
                               .arg(runningCount),
                           5000);
}

void MainWindow::onRemoteDataReceived(const QString& channelName,
                                      const QJsonObject& data) {
  // 实时数据更新 - 转发到仪表盘显示
  if (m_dashboardWidget && m_isRemoteMode) {
    m_dashboardWidget->updateRemoteData(channelName, data);
  }

  // 更新远程数据缓存
  m_remoteDataCache[channelName] = data;
}

}  // namespace ModbusPlexLink
