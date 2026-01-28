#include "ChannelConfigDialog.h"
#include "CollectorConfigDialog.h"
#include "ServerConfigDialog.h"
#include "DataMonitorWidget.h"
#include "DialogStyles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSplitter>
#include <QJsonArray>
#include <QDebug>
#include <QSizePolicy>
#include <QGraphicsDropShadowEffect>
#include <QFrame>

namespace ModbusPlexLink {

ChannelConfigDialog::ChannelConfigDialog(Channel* channel,
                                         ChannelManager* channelManager,
                                         QWidget *parent)
    : QDialog(parent)
    , m_channel(channel)
    , m_channelManager(channelManager)
    , m_isNewChannel(channel == nullptr)
    , m_tabWidget(nullptr)
    , m_nameEdit(nullptr)
    , m_enabledCheck(nullptr)
    , m_descriptionEdit(nullptr)
    , m_collectorTable(nullptr)
    , m_addCollectorBtn(nullptr)
    , m_editCollectorBtn(nullptr)
    , m_deleteCollectorBtn(nullptr)
    , m_collectorCountLabel(nullptr)
    , m_serverTable(nullptr)
    , m_addServerBtn(nullptr)
    , m_editServerBtn(nullptr)
    , m_deleteServerBtn(nullptr)
    , m_serverCountLabel(nullptr)
    , m_dataMonitorWidget(nullptr)
    , m_monitorInfoLabel(nullptr)
{
    setWindowTitle(m_isNewChannel ? tr("✨ 新建通道") : tr("📝 编辑通道"));
    resize(950, 720);
    setMinimumSize(850, 620);
    
    // 应用现代化样式
    setStyleSheet(DialogStyles::getDialogStyle());
    
    setupUi();
    loadConfig();
    
    // 新建通道时，设置默认名称
    if (m_isNewChannel) {
        m_nameEdit->setText(tr("新通道"));
        m_nameEdit->selectAll();
        m_nameEdit->setFocus();
    }
    
    // 禁用所有 SpinBox 和 ComboBox 的滚轮事件，防止意外修改
    DialogStyles::disableAllWheelEvents(this);
}

ChannelConfigDialog::~ChannelConfigDialog() {
}

void ChannelConfigDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);
    
    // 顶部标题区域
    QWidget* headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background: transparent;");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 10);
    
    QLabel* iconLabel = new QLabel(m_isNewChannel ? "🔗" : "⚙️", headerWidget);
    iconLabel->setStyleSheet("font-size: 28px;");
    headerLayout->addWidget(iconLabel);
    
    QLabel* titleLabel = new QLabel(m_isNewChannel ? tr("创建新的数据通道") : tr("编辑通道配置"), headerWidget);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #1E293B;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    
    QLabel* subtitleLabel = new QLabel(tr("配置采集器和服务器以建立数据流"), headerWidget);
    subtitleLabel->setStyleSheet("color: #64748B; font-size: 11px;");
    headerLayout->addWidget(subtitleLabel);
    
    mainLayout->addWidget(headerWidget);
    
    // Tab页签
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);
    
    createBasicInfoTab();
    createCollectorTab();
    createServerTab();
    createMonitorTab();
    
    // 设置 Tab 图标
    m_tabWidget->setTabText(0, tr("📋 基本信息"));
    m_tabWidget->setTabText(1, tr("📥 采集器"));
    m_tabWidget->setTabText(2, tr("📤 服务器"));
    m_tabWidget->setTabText(3, tr("📊 数据监控"));
    
    mainLayout->addWidget(m_tabWidget, 1);
    
    // 分隔线
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: #E2E8F0;");
    line->setFixedHeight(1);
    mainLayout->addWidget(line);
    
    // 底部按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    
    QPushButton* cancelBtn = DialogStyles::createSecondaryButton(tr("取消"), this);
    QPushButton* okBtn = DialogStyles::createPrimaryButton(m_isNewChannel ? tr("创建通道") : tr("保存更改"), this);
    
    connect(cancelBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onRejected);
    connect(okBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onAccepted);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(okBtn);
    
    mainLayout->addLayout(buttonLayout);
}

void ChannelConfigDialog::createBasicInfoTab() {
    QWidget* basicTab = new QWidget();
    basicTab->setStyleSheet("background-color: transparent;");
    QVBoxLayout* layout = new QVBoxLayout(basicTab);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(20);
    
    // 表单卡片
    QWidget* formCard = new QWidget(basicTab);
    formCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    QVBoxLayout* cardLayout = new QVBoxLayout(formCard);
    cardLayout->setContentsMargins(24, 24, 24, 24);
    cardLayout->setSpacing(20);
    
    // 卡片标题
    QLabel* cardTitle = new QLabel(tr("🔧 基本配置"), formCard);
    cardTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #1E293B; border: none; background: transparent;");
    cardLayout->addWidget(cardTitle);
    
    // 分隔线
    QFrame* sep = new QFrame(formCard);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background-color: #E2E8F0; border: none;");
    sep->setFixedHeight(1);
    cardLayout->addWidget(sep);
    
    // 表单
    QFormLayout* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setHorizontalSpacing(20);
    formLayout->setVerticalSpacing(16);
    
    // 通道名称
    m_nameEdit = new QLineEdit(formCard);
    m_nameEdit->setPlaceholderText(tr("请输入通道名称（唯一标识）"));
    m_nameEdit->setMaxLength(64);
    m_nameEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 8px; padding: 10px 14px; background: #F8FAFC;");
    connect(m_nameEdit, &QLineEdit::textChanged, this, &ChannelConfigDialog::onNameChanged);
    
    QLabel* nameLabel = new QLabel(tr("通道名称 *"), formCard);
    nameLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    formLayout->addRow(nameLabel, m_nameEdit);
    
    // 启用状态
    m_enabledCheck = new QCheckBox(tr("启用此通道（启用后自动开始数据采集）"), formCard);
    m_enabledCheck->setChecked(true);
    m_enabledCheck->setStyleSheet("border: none; background: transparent; color: #475569;");
    QLabel* enableLabel = new QLabel(tr("启用状态"), formCard);
    enableLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    formLayout->addRow(enableLabel, m_enabledCheck);
    
    cardLayout->addLayout(formLayout);
    
    // 描述区域
    QLabel* descLabel = new QLabel(tr("描述信息"), formCard);
    descLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    cardLayout->addWidget(descLabel);
    
    m_descriptionEdit = new QTextEdit(formCard);
    m_descriptionEdit->setPlaceholderText(tr("可选：输入通道的详细描述信息，如用途、关联设备等"));
    m_descriptionEdit->setMaximumHeight(100);
    m_descriptionEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 8px; padding: 10px; background: #F8FAFC;");
    cardLayout->addWidget(m_descriptionEdit);
    
    layout->addWidget(formCard);
    
    // 提示卡片
    QWidget* hintCard = new QWidget(basicTab);
    hintCard->setStyleSheet(R"(
        QWidget {
            background-color: #EFF6FF;
            border: 1px solid #BFDBFE;
            border-radius: 10px;
            border-left: 4px solid #3B82F6;
        }
    )");
    QHBoxLayout* hintLayout = new QHBoxLayout(hintCard);
    hintLayout->setContentsMargins(16, 14, 16, 14);
    
    QLabel* hintIcon = new QLabel("💡", hintCard);
    hintIcon->setStyleSheet("font-size: 18px; border: none; background: transparent;");
    hintLayout->addWidget(hintIcon);
    
    QLabel* hintLabel = new QLabel(
        tr("通道是数据采集和转发的独立单元，每个通道拥有独立的数据缓存(UDM)。\n"
           "采集器从设备读取数据写入UDM，服务器从UDM读取数据响应客户端。"),
        hintCard);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #1E40AF; font-size: 10pt; border: none; background: transparent;");
    hintLayout->addWidget(hintLabel, 1);
    
    layout->addWidget(hintCard);
    layout->addStretch();
    
    m_tabWidget->addTab(basicTab, tr("基本信息"));
}

void ChannelConfigDialog::createCollectorTab() {
    QWidget* collectorTab = new QWidget();
    collectorTab->setStyleSheet("background-color: transparent;");
    QVBoxLayout* layout = new QVBoxLayout(collectorTab);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);
    
    // 顶部卡片 - 标题和操作
    QWidget* headerCard = new QWidget(collectorTab);
    headerCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    QVBoxLayout* headerCardLayout = new QVBoxLayout(headerCard);
    headerCardLayout->setContentsMargins(20, 16, 20, 16);
    headerCardLayout->setSpacing(12);
    
    // 标题行
    QHBoxLayout* titleRow = new QHBoxLayout();
    
    QLabel* iconLabel = new QLabel("📥", headerCard);
    iconLabel->setStyleSheet("font-size: 20px; border: none; background: transparent;");
    titleRow->addWidget(iconLabel);
    
    QLabel* titleLabel = new QLabel(tr("数据采集器"), headerCard);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #1E293B; border: none; background: transparent;");
    titleRow->addWidget(titleLabel);
    
    titleRow->addStretch();
    
    m_collectorCountLabel = new QLabel(tr("0 个采集器"), collectorTab);
    m_collectorCountLabel->setStyleSheet(R"(
        color: #3B82F6;
        font-weight: 600;
        font-size: 10pt;
        padding: 4px 12px;
        background-color: #EFF6FF;
        border-radius: 12px;
        border: none;
    )");
    titleRow->addWidget(m_collectorCountLabel);
    
    headerCardLayout->addLayout(titleRow);
    
    // 说明文字
    QLabel* hintLabel = new QLabel(
        tr("采集器负责从物理设备（如PLC、仪表）读取数据并写入通道的UDM数据缓存。双击可编辑。"),
        headerCard);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #64748B; font-size: 9pt; border: none; background: transparent;");
    headerCardLayout->addWidget(hintLabel);
    
    // 操作按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    
    m_addCollectorBtn = DialogStyles::createPrimaryButton(tr("＋ 添加采集器"), collectorTab);
    m_editCollectorBtn = DialogStyles::createSecondaryButton(tr("✏️ 编辑"), collectorTab);
    m_deleteCollectorBtn = DialogStyles::createDangerButton(tr("🗑️ 删除"), collectorTab);
    
    connect(m_addCollectorBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onAddCollector);
    connect(m_editCollectorBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onEditCollector);
    connect(m_deleteCollectorBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onDeleteCollector);
    
    btnLayout->addWidget(m_addCollectorBtn);
    btnLayout->addWidget(m_editCollectorBtn);
    btnLayout->addWidget(m_deleteCollectorBtn);
    btnLayout->addStretch();
    
    headerCardLayout->addLayout(btnLayout);
    
    layout->addWidget(headerCard);
    
    // 采集器表格卡片
    QWidget* tableCard = new QWidget(collectorTab);
    tableCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    QVBoxLayout* tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(4, 4, 4, 4);
    
    m_collectorTable = new QTableWidget(tableCard);
    m_collectorTable->setColumnCount(6);
    m_collectorTable->setHorizontalHeaderLabels({
        tr("采集器名称"), tr("IP地址/串口"), tr("端口"), tr("从站ID"), 
        tr("轮询周期"), tr("点位数")
    });
    
    // 应用现代表格样式
    DialogStyles::setupModernTable(m_collectorTable);
    m_collectorTable->horizontalHeader()->setStretchLastSection(true);
    m_collectorTable->setColumnWidth(0, 160);
    m_collectorTable->setColumnWidth(1, 130);
    m_collectorTable->setColumnWidth(2, 70);
    m_collectorTable->setColumnWidth(3, 70);
    m_collectorTable->setColumnWidth(4, 90);
    
    connect(m_collectorTable, &QTableWidget::itemSelectionChanged,
            this, &ChannelConfigDialog::onCollectorTableSelectionChanged);
    connect(m_collectorTable, &QTableWidget::cellDoubleClicked,
            this, &ChannelConfigDialog::onCollectorTableDoubleClicked);
    
    tableCardLayout->addWidget(m_collectorTable);
    
    layout->addWidget(tableCard, 1);
    
    m_tabWidget->addTab(collectorTab, tr("采集器"));
    
    updateCollectorButtons();
}

void ChannelConfigDialog::createServerTab() {
    QWidget* serverTab = new QWidget();
    serverTab->setStyleSheet("background-color: transparent;");
    QVBoxLayout* layout = new QVBoxLayout(serverTab);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);
    
    // 顶部卡片 - 标题和操作
    QWidget* headerCard = new QWidget(serverTab);
    headerCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    QVBoxLayout* headerCardLayout = new QVBoxLayout(headerCard);
    headerCardLayout->setContentsMargins(20, 16, 20, 16);
    headerCardLayout->setSpacing(12);
    
    // 标题行
    QHBoxLayout* titleRow = new QHBoxLayout();
    
    QLabel* iconLabel = new QLabel("📤", headerCard);
    iconLabel->setStyleSheet("font-size: 20px; border: none; background: transparent;");
    titleRow->addWidget(iconLabel);
    
    QLabel* titleLabel = new QLabel(tr("Modbus服务器"), headerCard);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #1E293B; border: none; background: transparent;");
    titleRow->addWidget(titleLabel);
    
    titleRow->addStretch();
    
    m_serverCountLabel = new QLabel(tr("0 个服务器"), serverTab);
    m_serverCountLabel->setStyleSheet(R"(
        color: #059669;
        font-weight: 600;
        font-size: 10pt;
        padding: 4px 12px;
        background-color: #ECFDF5;
        border-radius: 12px;
        border: none;
    )");
    titleRow->addWidget(m_serverCountLabel);
    
    headerCardLayout->addLayout(titleRow);
    
    // 说明文字
    QLabel* hintLabel = new QLabel(
        tr("服务器监听客户端连接，从UDM读取数据并响应请求。每个服务器可包含多个虚拟设备（从站）。"),
        headerCard);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #64748B; font-size: 9pt; border: none; background: transparent;");
    headerCardLayout->addWidget(hintLabel);
    
    // 操作按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    
    m_addServerBtn = DialogStyles::createPrimaryButton(tr("＋ 添加服务器"), serverTab);
    m_editServerBtn = DialogStyles::createSecondaryButton(tr("✏️ 编辑"), serverTab);
    m_deleteServerBtn = DialogStyles::createDangerButton(tr("🗑️ 删除"), serverTab);
    
    connect(m_addServerBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onAddServer);
    connect(m_editServerBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onEditServer);
    connect(m_deleteServerBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onDeleteServer);
    
    btnLayout->addWidget(m_addServerBtn);
    btnLayout->addWidget(m_editServerBtn);
    btnLayout->addWidget(m_deleteServerBtn);
    btnLayout->addStretch();
    
    headerCardLayout->addLayout(btnLayout);
    
    layout->addWidget(headerCard);
    
    // 服务器表格卡片
    QWidget* tableCard = new QWidget(serverTab);
    tableCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    QVBoxLayout* tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(4, 4, 4, 4);
    
    m_serverTable = new QTableWidget(tableCard);
    m_serverTable->setColumnCount(5);
    m_serverTable->setHorizontalHeaderLabels({
        tr("服务器名称"), tr("监听地址"), tr("端口"), 
        tr("虚拟设备数"), tr("状态")
    });
    
    // 应用现代表格样式
    DialogStyles::setupModernTable(m_serverTable);
    m_serverTable->horizontalHeader()->setStretchLastSection(true);
    m_serverTable->setColumnWidth(0, 160);
    m_serverTable->setColumnWidth(1, 120);
    m_serverTable->setColumnWidth(2, 70);
    m_serverTable->setColumnWidth(3, 100);
    
    connect(m_serverTable, &QTableWidget::itemSelectionChanged,
            this, &ChannelConfigDialog::onServerTableSelectionChanged);
    connect(m_serverTable, &QTableWidget::cellDoubleClicked,
            this, &ChannelConfigDialog::onServerTableDoubleClicked);
    
    tableCardLayout->addWidget(m_serverTable);
    
    layout->addWidget(tableCard, 1);
    
    m_tabWidget->addTab(serverTab, tr("服务器"));
    
    updateServerButtons();
}

void ChannelConfigDialog::createMonitorTab() {
    QWidget* monitorTab = new QWidget();
    monitorTab->setStyleSheet("background-color: transparent;");
    QVBoxLayout* layout = new QVBoxLayout(monitorTab);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);
    
    // 头部卡片
    QWidget* headerCard = new QWidget(monitorTab);
    headerCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 16, 20, 16);
    
    QLabel* iconLabel = new QLabel("📊", headerCard);
    iconLabel->setStyleSheet("font-size: 20px; border: none; background: transparent;");
    headerLayout->addWidget(iconLabel);
    
    QLabel* titleLabel = new QLabel(tr("实时数据监控"), headerCard);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #1E293B; border: none; background: transparent;");
    headerLayout->addWidget(titleLabel);
    
    headerLayout->addStretch();
    
    // 状态指示
    const bool canPreview = (m_channelManager != nullptr && m_channel != nullptr);
    QLabel* statusLabel = new QLabel(canPreview ? tr("🟢 已连接") : tr("⚪ 未启动"), headerCard);
    statusLabel->setStyleSheet(QString(R"(
        color: %1;
        font-weight: 600;
        font-size: 10pt;
        padding: 4px 12px;
        background-color: %2;
        border-radius: 12px;
        border: none;
    )").arg(canPreview ? "#059669" : "#64748B").arg(canPreview ? "#ECFDF5" : "#F1F5F9"));
    headerLayout->addWidget(statusLabel);
    
    layout->addWidget(headerCard);
    
    // 信息提示卡片
    QString infoText;
    QString bgColor, borderColor, textColor, iconText;
    if (canPreview) {
        infoText = tr("实时预览当前通道的UDM数据，便于核对映射规则与报警配置。");
        bgColor = "#ECFDF5"; borderColor = "#A7F3D0"; textColor = "#065F46"; iconText = "✅";
    } else {
        infoText = tr("保存配置并在主界面启动通道后，可在此页实时查看采集的数据。");
        bgColor = "#FEF3C7"; borderColor = "#FCD34D"; textColor = "#92400E"; iconText = "⚠️";
    }
    
    QWidget* infoCard = new QWidget(monitorTab);
    infoCard->setStyleSheet(QString(R"(
        QWidget {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 10px;
            border-left: 4px solid %2;
        }
    )").arg(bgColor).arg(borderColor));
    QHBoxLayout* infoLayout = new QHBoxLayout(infoCard);
    infoLayout->setContentsMargins(16, 12, 16, 12);
    
    QLabel* infoIcon = new QLabel(iconText, infoCard);
    infoIcon->setStyleSheet("font-size: 16px; border: none; background: transparent;");
    infoLayout->addWidget(infoIcon);
    
    m_monitorInfoLabel = new QLabel(infoText, infoCard);
    m_monitorInfoLabel->setWordWrap(true);
    m_monitorInfoLabel->setStyleSheet(QString("color: %1; font-size: 10pt; border: none; background: transparent;").arg(textColor));
    infoLayout->addWidget(m_monitorInfoLabel, 1);
    
    layout->addWidget(infoCard);
    
    // 数据显示区域
    if (canPreview) {
        QWidget* dataCard = new QWidget(monitorTab);
        dataCard->setStyleSheet(R"(
            QWidget {
                background-color: white;
                border: 1px solid #E2E8F0;
                border-radius: 12px;
            }
        )");
        QVBoxLayout* dataLayout = new QVBoxLayout(dataCard);
        dataLayout->setContentsMargins(4, 4, 4, 4);
        
        m_dataMonitorWidget = new DataMonitorWidget(m_channelManager, dataCard);
        m_dataMonitorWidget->setChannel(m_channel);
        m_dataMonitorWidget->setMinimumHeight(320);
        dataLayout->addWidget(m_dataMonitorWidget);
        
        layout->addWidget(dataCard, 1);
    } else {
        // 占位符卡片
        QWidget* placeholderCard = new QWidget(monitorTab);
        placeholderCard->setStyleSheet(R"(
            QWidget {
                background-color: white;
                border: 2px dashed #CBD5E1;
                border-radius: 12px;
            }
        )");
        QVBoxLayout* placeholderLayout = new QVBoxLayout(placeholderCard);
        placeholderLayout->setContentsMargins(40, 60, 40, 60);
        
        QLabel* emptyIcon = new QLabel("📭", placeholderCard);
        emptyIcon->setAlignment(Qt::AlignCenter);
        emptyIcon->setStyleSheet("font-size: 48px; border: none; background: transparent;");
        placeholderLayout->addWidget(emptyIcon);
        
        QLabel* emptyTitle = new QLabel(tr("暂无实时数据"), placeholderCard);
        emptyTitle->setAlignment(Qt::AlignCenter);
        emptyTitle->setStyleSheet("font-size: 14pt; font-weight: bold; color: #475569; border: none; background: transparent;");
        placeholderLayout->addWidget(emptyTitle);
        
        QLabel* emptyText = new QLabel(tr("请先保存配置并在主界面启动通道"), placeholderCard);
        emptyText->setAlignment(Qt::AlignCenter);
        emptyText->setStyleSheet("font-size: 10pt; color: #94A3B8; border: none; background: transparent;");
        placeholderLayout->addWidget(emptyText);
        
        placeholderLayout->addStretch();
        
        layout->addWidget(placeholderCard, 1);
    }
    
    m_tabWidget->addTab(monitorTab, tr("数据监控"));
}

void ChannelConfigDialog::loadConfig() {
    if (m_channel) {
        // 本地模式编辑：从本地通道对象获取配置
        m_config = m_channel->getConfig();
        
        // 加载基本信息
        m_nameEdit->setText(m_config.name);
        m_nameEdit->setEnabled(false);  // 编辑模式下不允许修改名称
        m_enabledCheck->setChecked(m_config.enabled);
        
        // 加载采集器和服务器配置
        m_collectors = m_config.collectors;
        m_servers = m_config.servers;
    } else if (!m_config.name.isEmpty()) {
        // 远程模式编辑：m_config 已通过 setConfig() 设置
        // 加载基本信息
        m_nameEdit->setText(m_config.name);
        m_nameEdit->setEnabled(false);  // 编辑模式下不允许修改名称
        m_enabledCheck->setChecked(m_config.enabled);
        
        // 加载采集器和服务器配置
        m_collectors = m_config.collectors;
        m_servers = m_config.servers;
    } else {
        // 新建通道，使用默认值
        m_config.enabled = true;
    }
    
    refreshCollectorTable();
    refreshServerTable();
}

bool ChannelConfigDialog::validateConfig() {
    // 验证通道名称
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("通道名称不能为空"));
        m_tabWidget->setCurrentIndex(0);
        m_nameEdit->setFocus();
        return false;
    }
    
    // 验证是否至少有一个采集器
    if (m_collectors.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("确认"),
            tr("当前没有配置采集器，通道将无法采集数据。\n是否继续？"),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            m_tabWidget->setCurrentIndex(1);
            return false;
        }
    }
    
    // 验证是否至少有一个服务器
    if (m_servers.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("确认"),
            tr("当前没有配置服务器，通道将无法对外提供数据。\n是否继续？"),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            m_tabWidget->setCurrentIndex(2);
            return false;
        }
    }
    
    return true;
}

ChannelConfig ChannelConfigDialog::getConfig() const {
    ChannelConfig config;
    config.name = m_nameEdit->text().trimmed();
    config.enabled = m_enabledCheck->isChecked();
    config.collectors = m_collectors;
    config.servers = m_servers;
    return config;
}

void ChannelConfigDialog::setConfig(const ChannelConfig& config) {
    m_config = config;
    loadConfig();
}

void ChannelConfigDialog::refreshCollectorTable() {
    m_collectorTable->setRowCount(0);
    m_collectorTable->setRowCount(m_collectors.size());
    
    for (int i = 0; i < m_collectors.size(); ++i) {
        QJsonObject collector = m_collectors[i];
        
        // 名称
        QTableWidgetItem* nameItem = new QTableWidgetItem(
            collector.value("name").toString());
        m_collectorTable->setItem(i, 0, nameItem);
        
        // IP地址
        QTableWidgetItem* ipItem = new QTableWidgetItem(
            collector.value("ip").toString());
        ipItem->setTextAlignment(Qt::AlignCenter);
        m_collectorTable->setItem(i, 1, ipItem);
        
        // 端口
        QTableWidgetItem* portItem = new QTableWidgetItem(
            QString::number(collector.value("port").toInt()));
        portItem->setTextAlignment(Qt::AlignCenter);
        m_collectorTable->setItem(i, 2, portItem);
        
        // 从站ID
        QTableWidgetItem* unitIdItem = new QTableWidgetItem(
            QString::number(collector.value("unitId").toInt()));
        unitIdItem->setTextAlignment(Qt::AlignCenter);
        m_collectorTable->setItem(i, 3, unitIdItem);
        
        // 轮询间隔
        QTableWidgetItem* pollRateItem = new QTableWidgetItem(
            QString::number(collector.value("pollRate").toInt()));
        pollRateItem->setTextAlignment(Qt::AlignCenter);
        m_collectorTable->setItem(i, 4, pollRateItem);
        
        // 映射数量
        int mappingCount = collector.value("mappings").toArray().size();
        QTableWidgetItem* mappingCountItem = new QTableWidgetItem(
            QString::number(mappingCount));
        mappingCountItem->setTextAlignment(Qt::AlignCenter);
        m_collectorTable->setItem(i, 5, mappingCountItem);
    }
    
    m_collectorCountLabel->setText(tr("%1 个采集器").arg(m_collectors.size()));
    updateCollectorButtons();
}

void ChannelConfigDialog::refreshServerTable() {
    m_serverTable->setRowCount(0);
    m_serverTable->setRowCount(m_servers.size());
    
    for (int i = 0; i < m_servers.size(); ++i) {
        QJsonObject server = m_servers[i];
        
        // 名称
        QTableWidgetItem* nameItem = new QTableWidgetItem(
            server.value("name").toString());
        m_serverTable->setItem(i, 0, nameItem);
        
        // 监听地址
        QTableWidgetItem* listenItem = new QTableWidgetItem(
            server.value("listenAddress").toString("0.0.0.0"));
        listenItem->setTextAlignment(Qt::AlignCenter);
        m_serverTable->setItem(i, 1, listenItem);
        
        // 端口
        QTableWidgetItem* portItem = new QTableWidgetItem(
            QString::number(server.value("port").toInt()));
        portItem->setTextAlignment(Qt::AlignCenter);
        m_serverTable->setItem(i, 2, portItem);
        
        // 虚拟设备数量
        int vdCount = server.value("virtualDevices").toArray().size();
        QTableWidgetItem* vdCountItem = new QTableWidgetItem(
            QString::number(vdCount));
        vdCountItem->setTextAlignment(Qt::AlignCenter);
        m_serverTable->setItem(i, 3, vdCountItem);
        
        // 状态
        QTableWidgetItem* statusItem = new QTableWidgetItem(tr("已配置"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(QBrush(QColor(100, 100, 100)));
        m_serverTable->setItem(i, 4, statusItem);
    }
    
    m_serverCountLabel->setText(tr("%1 个服务器").arg(m_servers.size()));
    updateServerButtons();
}

int ChannelConfigDialog::getSelectedCollectorRow() const {
    QList<QTableWidgetItem*> selected = m_collectorTable->selectedItems();
    if (selected.isEmpty()) {
        return -1;
    }
    return selected.first()->row();
}

int ChannelConfigDialog::getSelectedServerRow() const {
    QList<QTableWidgetItem*> selected = m_serverTable->selectedItems();
    if (selected.isEmpty()) {
        return -1;
    }
    return selected.first()->row();
}

void ChannelConfigDialog::updateCollectorButtons() {
    bool hasSelection = getSelectedCollectorRow() >= 0;
    m_editCollectorBtn->setEnabled(hasSelection);
    m_deleteCollectorBtn->setEnabled(hasSelection);
}

void ChannelConfigDialog::updateServerButtons() {
    bool hasSelection = getSelectedServerRow() >= 0;
    m_editServerBtn->setEnabled(hasSelection);
    m_deleteServerBtn->setEnabled(hasSelection);
}

// ============= 槽函数实现 =============

void ChannelConfigDialog::onNameChanged(const QString& text) {
    Q_UNUSED(text);
    // 实时验证可以在这里添加
}

void ChannelConfigDialog::onAddCollector() {
    CollectorConfigDialog dialog(QJsonObject(), this);
    dialog.setWindowTitle(tr("新建采集器"));
    
    if (dialog.exec() == QDialog::Accepted) {
        QJsonObject config = dialog.getConfig();
        m_collectors.append(config);
        refreshCollectorTable();
    }
}

void ChannelConfigDialog::onEditCollector() {
    int row = getSelectedCollectorRow();
    if (row < 0 || row >= m_collectors.size()) {
        return;
    }
    
    CollectorConfigDialog dialog(m_collectors[row], this);
    dialog.setWindowTitle(tr("编辑采集器 - %1")
        .arg(m_collectors[row].value("name").toString()));
    
    if (dialog.exec() == QDialog::Accepted) {
        QJsonObject config = dialog.getConfig();
        m_collectors[row] = config;
        refreshCollectorTable();
    }
}

void ChannelConfigDialog::onDeleteCollector() {
    int row = getSelectedCollectorRow();
    if (row < 0 || row >= m_collectors.size()) {
        return;
    }
    
    QString collectorName = m_collectors[row].value("name").toString();
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("确认删除"),
        tr("确定要删除采集器 '%1' 吗？").arg(collectorName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        m_collectors.removeAt(row);
        refreshCollectorTable();
    }
}

void ChannelConfigDialog::onCollectorTableSelectionChanged() {
    updateCollectorButtons();
}

void ChannelConfigDialog::onCollectorTableDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row >= 0) {
        onEditCollector();
    }
}

void ChannelConfigDialog::onAddServer() {
    ServerConfigDialog dialog(QJsonObject(), this);
    dialog.setWindowTitle(tr("新建服务器"));
    dialog.setAvailableVariables(getAvailableVariables());

    if (dialog.exec() == QDialog::Accepted) {
        QJsonObject config = dialog.getConfig();
        m_servers.append(config);
        refreshServerTable();
    }
}

void ChannelConfigDialog::onEditServer() {
    int row = getSelectedServerRow();
    if (row < 0 || row >= m_servers.size()) {
        return;
    }

    ServerConfigDialog dialog(m_servers[row], this);
    dialog.setWindowTitle(tr("编辑服务器 - %1")
        .arg(m_servers[row].value("name").toString()));
    dialog.setAvailableVariables(getAvailableVariables());

    if (dialog.exec() == QDialog::Accepted) {
        QJsonObject config = dialog.getConfig();
        m_servers[row] = config;
        refreshServerTable();
    }
}

QList<AvailableVariable> ChannelConfigDialog::getAvailableVariables() const {
    QList<AvailableVariable> variables;
    
    // 遍历所有采集器配置，提取变量
    for (const QJsonObject& collectorConfig : m_collectors) {
        QString collectorName = collectorConfig.value("name").toString();
        QJsonArray mappings = collectorConfig.value("mappings").toArray();
        
        for (const QJsonValue& mappingVal : mappings) {
            QJsonObject mapping = mappingVal.toObject();
            
            AvailableVariable var;
            var.collectorName = collectorName;
            var.tagName = mapping.value("tagName").toString();
            var.fullId = QString("%1:%2").arg(collectorName).arg(var.tagName);
            var.comment = mapping.value("comment").toString();
            
            // 解析数据类型
            QString dataTypeStr = mapping.value("dataType").toString("UInt16");
            var.dataType = DataTypeUtils::dataTypeFromString(dataTypeStr);
            
            if (!var.tagName.isEmpty()) {
                variables.append(var);
            }
        }
    }
    
    return variables;
}

void ChannelConfigDialog::onDeleteServer() {
    int row = getSelectedServerRow();
    if (row < 0 || row >= m_servers.size()) {
        return;
    }
    
    QString serverName = m_servers[row].value("name").toString();
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("确认删除"),
        tr("确定要删除服务器 '%1' 吗？").arg(serverName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        m_servers.removeAt(row);
        refreshServerTable();
    }
}

void ChannelConfigDialog::onServerTableSelectionChanged() {
    updateServerButtons();
}

void ChannelConfigDialog::onServerTableDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row >= 0) {
        onEditServer();
    }
}

void ChannelConfigDialog::onAccepted() {
    if (validateConfig()) {
        accept();
    }
}

void ChannelConfigDialog::onRejected() {
    reject();
}

} // namespace ModbusPlexLink
