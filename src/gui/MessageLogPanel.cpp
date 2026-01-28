#include "MessageLogPanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include <QScrollBar>
#include <QRegularExpression>
#include <cmath>
#include <cstring>

namespace ModbusPlexLink {

// ============================================================================
// ModbusMessageViewer 实现
// ============================================================================

ModbusMessageViewer::ModbusMessageViewer(QWidget *parent)
    : QWidget(parent)
    , m_channel(nullptr)
    , m_titleLabel(nullptr)
    , m_modeLabel(nullptr)
    , m_paused(false)
    , m_showDetail(true)
    , m_messageCount(0)
    , m_isRemoteMode(false)
{
    setupUi();
}

void ModbusMessageViewer::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    // 工具栏
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setContentsMargins(8, 8, 8, 0);
    toolbarLayout->setSpacing(8);

    // 标题标签（动态显示通道名称）
    m_titleLabel = new QLabel(tr("📡 Modbus报文"), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(10);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    toolbarLayout->addWidget(m_titleLabel);
    
    // 模式指示标签
    m_modeLabel = new QLabel(this);
    m_modeLabel->setObjectName("modeLabel");
    updateModeLabel();
    toolbarLayout->addWidget(m_modeLabel);

    toolbarLayout->addStretch();

    m_pauseCheck = new QCheckBox(tr("暂停"), this);
    connect(m_pauseCheck, &QCheckBox::toggled,
            this, &ModbusMessageViewer::onPauseToggled);
    toolbarLayout->addWidget(m_pauseCheck);

    m_detailCheck = new QCheckBox(tr("详细"), this);
    m_detailCheck->setChecked(true);
    connect(m_detailCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_showDetail = checked;
    });
    toolbarLayout->addWidget(m_detailCheck);

    m_clearBtn = new QPushButton(tr("清空"), this);
    m_clearBtn->setObjectName("toolButton");
    connect(m_clearBtn, &QPushButton::clicked,
            this, &ModbusMessageViewer::onClearClicked);
    toolbarLayout->addWidget(m_clearBtn);

    m_exportBtn = new QPushButton(tr("导出"), this);
    m_exportBtn->setObjectName("toolButton");
    connect(m_exportBtn, &QPushButton::clicked,
            this, &ModbusMessageViewer::onExportClicked);
    toolbarLayout->addWidget(m_exportBtn);

    mainLayout->addLayout(toolbarLayout);

    // 报文显示区域
    m_messageDisplay = new QTextBrowser(this);
    m_messageDisplay->setObjectName("messageDisplay");
    m_messageDisplay->setOpenExternalLinks(false);
    mainLayout->addWidget(m_messageDisplay);

    // 样式
    setStyleSheet(R"(
        #messageDisplay {
            background-color: #1F2937;
            color: #F3F4F6;
            border: 1px solid #374151;
            border-radius: 6px;
            padding: 8px;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 9pt;
        }
        #toolButton {
            background-color: white;
            border: 1px solid #E5E7EB;
            border-radius: 4px;
            padding: 4px 12px;
            color: #374151;
        }
        #toolButton:hover {
            background-color: #F3F4F6;
            border-color: #3B82F6;
        }
        QCheckBox {
            color: #374151;
        }
        #modeLabel {
            padding: 2px 8px;
            border-radius: 4px;
            font-size: 9pt;
            font-weight: bold;
        }
    )");
}

void ModbusMessageViewer::updateModeLabel() {
    if (!m_modeLabel || !m_titleLabel) return;
    
    if (m_isRemoteMode) {
        QString channelText = m_remoteChannelName.isEmpty() ? tr("未选择通道") : m_remoteChannelName;
        m_titleLabel->setText(tr("📡 Modbus报文 - 远程通道: %1").arg(channelText));
        
        QString text = tr("🌐 远程");
        if (!m_remoteChannelName.isEmpty()) {
            text += QString(" [%1]").arg(m_remoteChannelName);
        }
        m_modeLabel->setText(text);
        m_modeLabel->setStyleSheet(R"(
            #modeLabel {
                background-color: #DBEAFE;
                color: #1D4ED8;
                padding: 2px 8px;
                border-radius: 4px;
                font-size: 9pt;
                font-weight: bold;
            }
        )");
    } else {
        QString channelText = m_channel ? m_channel->getName() : tr("未选择通道");
        m_titleLabel->setText(tr("📡 Modbus报文 - 本地通道: %1").arg(channelText));
        
        QString text = tr("💻 本地");
        if (m_channel) {
            text += QString(" [%1]").arg(m_channel->getName());
        }
        m_modeLabel->setText(text);
        m_modeLabel->setStyleSheet(R"(
            #modeLabel {
                background-color: #D1FAE5;
                color: #065F46;
                padding: 2px 8px;
                border-radius: 4px;
                font-size: 9pt;
                font-weight: bold;
            }
        )");
    }
}

void ModbusMessageViewer::setRemoteMode(bool remote, const QString& channelName) {
    // 如果模式切换，清除旧报文
    if (m_isRemoteMode != remote) {
        clearMessages();
    }
    
    m_isRemoteMode = remote;
    m_remoteChannelName = channelName;
    
    // 切换到远程模式时，断开本地通道
    if (remote) {
        m_channel = nullptr;
    }
    
    updateModeLabel();
}

void ModbusMessageViewer::addMessage(const QString& direction, const QString& device,
                                     const QString& function, const QString& address,
                                     const QString& data, bool success) {
    if (m_paused) return;

    m_messageCount++;
    QString html = formatMessage(direction, device, function, address, data, success);
    m_messageDisplay->append(html);

    // 限制最大行数
    if (m_messageCount > 1000) {
        m_messageDisplay->clear();
        m_messageCount = 0;
    }
}

void ModbusMessageViewer::clearMessages() {
    m_messageDisplay->clear();
    m_messageCount = 0;
}

void ModbusMessageViewer::addStatusMessage(const QString& message, const QString& color) {
    if (m_paused) return;
    
    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss.zzz]");
    QString html = QString(
        "<div style='padding: 4px 0; margin: 2px 0;'>"
        "<span style='color: #6B7280;'>%1</span> "
        "<span style='color: %2; font-weight: bold;'>%3</span>"
        "</div>"
    ).arg(timestamp, color, message);
    
    m_messageDisplay->append(html);
    m_messageCount++;
}

void ModbusMessageViewer::setChannel(Channel* channel) {
    // 断开旧通道的状态信号
    if (m_channel) {
        disconnect(m_channel, &Channel::stateChanged, this, nullptr);
    }
    
    m_channel = channel;
    m_isRemoteMode = false;  // 设置通道意味着切换到本地模式
    m_remoteChannelName.clear();
    clearMessages();
    updateModeLabel();
    
    // 连接新通道的状态变化信号
    if (m_channel) {
        // 先断开可能存在的旧连接
        disconnect(m_channel, &Channel::stateChanged, this, nullptr);
        
        connect(m_channel, &Channel::stateChanged, this, [this]() {
            if (!m_channel) return;
            
            QString statusMsg;
            QString color;
            
            switch (m_channel->getState()) {
                case ChannelState::Running:
                    statusMsg = tr("🟢 通道 '%1' 已启动，开始采集数据").arg(m_channel->getName());
                    color = "#10B981";
                    break;
                case ChannelState::Stopped:
                    statusMsg = tr("⚫ 通道 '%1' 已停止").arg(m_channel->getName());
                    color = "#6B7280";
                    break;
                case ChannelState::Error:
                    statusMsg = tr("🔴 通道 '%1' 发生错误").arg(m_channel->getName());
                    color = "#EF4444";
                    break;
                case ChannelState::Starting:
                    statusMsg = tr("🟡 通道 '%1' 正在启动...").arg(m_channel->getName());
                    color = "#F59E0B";
                    break;
                case ChannelState::Stopping:
                    statusMsg = tr("🟡 通道 '%1' 正在停止...").arg(m_channel->getName());
                    color = "#F59E0B";
                    break;
            }
            
            if (!statusMsg.isEmpty()) {
                addStatusMessage(statusMsg, color);
            }
        });
        
        // 显示当前状态
        QString initialMsg;
        if (m_channel->isRunning()) {
            initialMsg = tr("📡 已连接到通道 '%1' (运行中)").arg(m_channel->getName());
        } else {
            initialMsg = tr("📡 已连接到通道 '%1' (已停止)").arg(m_channel->getName());
        }
        addStatusMessage(initialMsg, "#3B82F6");
    }
}

void ModbusMessageViewer::onClearClicked() {
    clearMessages();
}

void ModbusMessageViewer::onExportClicked() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("导出报文日志"),
        QDateTime::currentDateTime().toString("messages_yyyyMMdd_hhmmss.txt"),
        tr("文本文件 (*.txt);;所有文件 (*.*)"));

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"),
                           tr("无法写入文件: %1").arg(fileName));
        return;
    }

    QTextStream out(&file);
    out << m_messageDisplay->toPlainText();
    file.close();

    QMessageBox::information(this, tr("导出成功"),
                           tr("报文日志已导出到: %1").arg(fileName));
}

void ModbusMessageViewer::onPauseToggled(bool paused) {
    m_paused = paused;
}

QString ModbusMessageViewer::formatMessage(const QString& direction, const QString& device,
                                          const QString& function, const QString& address,
                                          const QString& data, bool success) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString directionIcon = (direction == "TX") ? "⬆ TX" : "⬇ RX";
    QString directionColor = (direction == "TX") ? "#60A5FA" : "#10B981";  // 蓝色发送，绿色接收
    QString statusColor = success ? "#10B981" : "#EF4444";
    QString statusIcon = success ? "✓" : "✗";
    
    // 解析功能码提示
    QString funcHint;
    QString funcCode;
    if (function.contains("01") && function.contains("线圈")) {
        funcHint = "读线圈(FC01)";
        funcCode = "01";
    } else if (function.contains("02") || function.contains("离散输入")) {
        funcHint = "读离散输入(FC02)";
        funcCode = "02";
    } else if (function.contains("03") || (function.contains("保持寄存器") && !function.contains("写"))) {
        funcHint = "读保持寄存器(FC03)";
        funcCode = "03";
    } else if (function.contains("04") || function.contains("输入寄存器")) {
        funcHint = "读输入寄存器(FC04)";
        funcCode = "04";
    } else if (function.contains("05") || (function.contains("线圈") && function.contains("写"))) {
        funcHint = "写单线圈(FC05)";
        funcCode = "05";
    } else if (function.contains("06") || (function.contains("保持寄存器") && function.contains("写"))) {
        funcHint = "写单寄存器(FC06)";
        funcCode = "06";
    } else if (function.contains("0F")) {
        funcHint = "写多线圈(FC0F)";
        funcCode = "0F";
    } else if (function.contains("10")) {
        funcHint = "写多寄存器(FC10)";
        funcCode = "10";
    } else {
        funcHint = function;
        funcCode = "??";
    }
    
    // 解析地址范围
    QString addrInfo = address;
    if (address.contains("~")) {
        QStringList parts = address.split("~");
        if (parts.size() == 2) {
            int start = parts[0].toInt();
            int end = parts[1].toInt();
            int count = end - start + 1;
            addrInfo = QString("%1~%2 (%3个)").arg(start).arg(end).arg(count);
        }
    }
    
    // 构建消息
    QString result = QString(
        "<span style='color: #6B7280;'>[%1]</span> "
        "<span style='color: %2; font-weight: bold;'>%3</span> "
        "<span style='color: #F59E0B;'>%4</span> "
        "<span style='color: #EC4899;'>%5</span> "
        "<span style='color: #8B5CF6;'>@%6</span> "
        "<span style='color: %7;'>%8</span> "
    ).arg(timestamp)
     .arg(directionColor)
     .arg(directionIcon)
     .arg(device)
     .arg(funcHint)
     .arg(addrInfo)
     .arg(statusColor)
     .arg(statusIcon);
    
    // 添加数据（带详细解析）
    if (m_showDetail && direction == "RX" && success && !data.contains("错误") && !data.contains("OK")) {
        QString parsedData = parseHexData(data);
        result += QString("<span style='color: #D1D5DB;'>%1</span>").arg(parsedData);
    } else {
        result += QString("<span style='color: #D1D5DB;'>%1</span>").arg(data);
    }
    
    return result;
}

QString ModbusMessageViewer::parseHexData(const QString& hexData) {
    // 解析十六进制数据并添加详细信息
    QString result = hexData;
    
    // 如果已包含解析信息，直接返回
    if (hexData.contains("[DEC:") || hexData.contains("[U16:")) {
        return result;
    }
    
    // 尝试解析纯十六进制数据
    QStringList hexParts = hexData.split(" ", Qt::SkipEmptyParts);
    if (hexParts.isEmpty()) {
        return result;
    }
    
    // 提取有效的4位十六进制值
    QStringList validHex;
    for (const QString& part : hexParts) {
        QString cleaned = part.trimmed();
        if (cleaned.length() == 4 && cleaned.contains(QRegularExpression("^[0-9A-Fa-f]+$"))) {
            validHex << cleaned;
        }
    }
    
    if (validHex.isEmpty()) {
        return result;  // 无法解析，返回原始数据
    }
    
    // 转换为十进制值列表
    QList<quint16> decValues;
    for (const QString& hex : validHex) {
        bool ok;
        quint16 val = hex.toUInt(&ok, 16);
        if (ok) {
            decValues << val;
        }
    }
    
    if (decValues.isEmpty()) {
        return result;  // 无法解析，返回原始数据
    }
    
    // 添加U16解析（显示所有值）
    QStringList decStrings;
    for (quint16 val : decValues) {
        decStrings << QString::number(val);
    }
    result += QString(" [U16: %1]").arg(decStrings.join(", "));
    
    // 如果是2个或4个寄存器，尝试解析为32位/64位值
    if (decValues.size() == 2) {
        quint32 u32_ab = (static_cast<quint32>(decValues[0]) << 16) | decValues[1];  // ABCD
        quint32 u32_cd = (static_cast<quint32>(decValues[1]) << 16) | decValues[0];  // CDAB
        
        // Float32解析
        float f32_ab, f32_cd;
        memcpy(&f32_ab, &u32_ab, sizeof(float));
        memcpy(&f32_cd, &u32_cd, sizeof(float));
        
        // 只显示合理范围内的浮点数
        if (qAbs(f32_ab) > 1e-6 && qAbs(f32_ab) < 1e10 && !std::isnan(f32_ab) && !std::isinf(f32_ab)) {
            result += QString(" [F32AB: %1]").arg(f32_ab, 0, 'g', 6);
        }
        if (qAbs(f32_cd) > 1e-6 && qAbs(f32_cd) < 1e10 && !std::isnan(f32_cd) && !std::isinf(f32_cd)) {
            result += QString(" [F32CD: %1]").arg(f32_cd, 0, 'g', 6);
        }
        
        result += QString(" [U32AB: %1]").arg(u32_ab);
    }
    
    return result;
}

// ============================================================================
// MessageLogPanel 实现
// ============================================================================

MessageLogPanel::MessageLogPanel(QWidget *parent)
    : QWidget(parent)
    , m_channel(nullptr)
    , m_isRemoteMode(false)
{
    setupUi();
}

MessageLogPanel::~MessageLogPanel() {
}

void MessageLogPanel::setupUi() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 左侧切换按钮栏
    QWidget* sideBar = new QWidget(this);
    sideBar->setObjectName("sideBar");
    sideBar->setFixedWidth(60);

    QVBoxLayout* sideLayout = new QVBoxLayout(sideBar);
    sideLayout->setContentsMargins(4, 8, 4, 8);
    sideLayout->setSpacing(4);

    m_buttonGroup = new QButtonGroup(this);

    // 报文按钮
    m_messageBtn = new QPushButton("📡\n报文", sideBar);
    m_messageBtn->setObjectName("sideButton");
    m_messageBtn->setCheckable(true);
    m_messageBtn->setChecked(true);
    m_messageBtn->setFixedHeight(60);
    m_messageBtn->setCursor(Qt::PointingHandCursor);
    connect(m_messageBtn, &QPushButton::clicked,
            this, &MessageLogPanel::onMessageModeClicked);
    m_buttonGroup->addButton(m_messageBtn, 0);
    sideLayout->addWidget(m_messageBtn);

    // 日志按钮
    m_logBtn = new QPushButton("📋\n日志", sideBar);
    m_logBtn->setObjectName("sideButton");
    m_logBtn->setCheckable(true);
    m_logBtn->setFixedHeight(60);
    m_logBtn->setCursor(Qt::PointingHandCursor);
    connect(m_logBtn, &QPushButton::clicked,
            this, &MessageLogPanel::onLogModeClicked);
    m_buttonGroup->addButton(m_logBtn, 1);
    sideLayout->addWidget(m_logBtn);

    sideLayout->addStretch();

    mainLayout->addWidget(sideBar);

    // 右侧内容区域
    m_stackedWidget = new QStackedWidget(this);

    m_messageViewer = new ModbusMessageViewer(this);
    m_stackedWidget->addWidget(m_messageViewer);

    m_logViewer = new LogViewerWidget(this);
    m_stackedWidget->addWidget(m_logViewer);

    mainLayout->addWidget(m_stackedWidget, 1);

    // 样式
    setStyleSheet(R"(
        #sideBar {
            background-color: #F3F4F6;
            border-right: 1px solid #E5E7EB;
        }
        #sideButton {
            background-color: transparent;
            border: none;
            border-radius: 6px;
            color: #6B7280;
            font-size: 9pt;
            text-align: center;
            padding: 8px;
        }
        #sideButton:hover {
            background-color: #E5E7EB;
        }
        #sideButton:checked {
            background-color: #3B82F6;
            color: white;
        }
    )");

    // 默认显示报文
    setActiveButton(m_messageBtn);
}

void MessageLogPanel::setChannel(Channel* channel) {
    m_channel = channel;
    m_isRemoteMode = false;
    m_remoteChannelName.clear();

    if (m_messageViewer) {
        m_messageViewer->setChannel(channel);
    }

    // 日志查看器是全局的，不需要设置通道
}

void MessageLogPanel::setRemoteMode(bool remote, const QString& channelName) {
    m_isRemoteMode = remote;
    m_remoteChannelName = channelName;
    
    if (remote) {
        m_channel = nullptr;
    }
    
    if (m_messageViewer) {
        m_messageViewer->setRemoteMode(remote, channelName);
    }
}

void MessageLogPanel::onMessageModeClicked() {
    m_stackedWidget->setCurrentIndex(0);
    setActiveButton(m_messageBtn);
}

void MessageLogPanel::onLogModeClicked() {
    m_stackedWidget->setCurrentIndex(1);
    setActiveButton(m_logBtn);
}

void MessageLogPanel::setActiveButton(QPushButton* button) {
    // 样式由 checkable + checked 状态自动处理
    button->setChecked(true);
}

} // namespace ModbusPlexLink
