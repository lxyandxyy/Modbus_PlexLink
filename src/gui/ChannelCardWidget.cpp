#include "ChannelCardWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace ModbusPlexLink {

ChannelCardWidget::ChannelCardWidget(Channel* channel, QWidget *parent)
    : QFrame(parent)
    , m_channel(channel)
    , m_selected(false)
    , m_hovered(false)
    , m_animationPhase(0)
{
    setObjectName("channelCard");
    setupUi();
    applyStyles();

    // 连接通道信号
    if (m_channel) {
        connect(m_channel, &Channel::stateChanged,
                this, &ChannelCardWidget::onChannelStateChanged);
    }

    // 动画定时器
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout,
            this, &ChannelCardWidget::updateIndicatorAnimation);
    m_animationTimer->start(500);

    updateDisplay();
}

ChannelCardWidget::~ChannelCardWidget() {
}

void ChannelCardWidget::setupUi() {
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(8);

    // 顶部：名称和状态指示器
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);

    m_statusIndicator = new QLabel(this);
    m_statusIndicator->setFixedSize(12, 12);
    m_statusIndicator->setObjectName("statusIndicator");
    headerLayout->addWidget(m_statusIndicator);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName("channelName");
    QFont nameFont = m_nameLabel->font();
    nameFont.setPointSize(11);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    headerLayout->addWidget(m_nameLabel, 1);

    m_startStopBtn = new QPushButton(this);
    m_startStopBtn->setObjectName("iconButton");
    m_startStopBtn->setFixedSize(28, 28);
    m_startStopBtn->setCursor(Qt::PointingHandCursor);
    connect(m_startStopBtn, &QPushButton::clicked,
            this, &ChannelCardWidget::onStartStopClicked);
    headerLayout->addWidget(m_startStopBtn);

    m_editBtn = new QPushButton("✎", this);
    m_editBtn->setObjectName("iconButton");
    m_editBtn->setFixedSize(28, 28);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setToolTip(tr("编辑通道"));
    connect(m_editBtn, &QPushButton::clicked,
            this, &ChannelCardWidget::onEditClicked);
    headerLayout->addWidget(m_editBtn);

    m_deleteBtn = new QPushButton("🗑", this);
    m_deleteBtn->setObjectName("iconButton");
    m_deleteBtn->setFixedSize(28, 28);
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setToolTip(tr("删除通道"));
    connect(m_deleteBtn, &QPushButton::clicked,
            this, &ChannelCardWidget::onDeleteClicked);
    headerLayout->addWidget(m_deleteBtn);

    mainLayout->addLayout(headerLayout);

    // 状态文本
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("statusText");
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(9);
    m_statusLabel->setFont(statusFont);
    mainLayout->addWidget(m_statusLabel);

    // 连接信息
    m_connectionLabel = new QLabel(this);
    m_connectionLabel->setObjectName("infoText");
    QFont infoFont = m_connectionLabel->font();
    infoFont.setPointSize(8);
    m_connectionLabel->setFont(infoFont);
    mainLayout->addWidget(m_connectionLabel);

    // 统计信息
    QHBoxLayout* statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(12);

    m_collectorCountLabel = new QLabel(this);
    m_collectorCountLabel->setObjectName("statLabel");
    QFont statFont = m_collectorCountLabel->font();
    statFont.setPointSize(8);
    m_collectorCountLabel->setFont(statFont);
    statsLayout->addWidget(m_collectorCountLabel);

    m_serverCountLabel = new QLabel(this);
    m_serverCountLabel->setObjectName("statLabel");
    m_serverCountLabel->setFont(statFont);
    statsLayout->addWidget(m_serverCountLabel);

    statsLayout->addStretch();
    mainLayout->addLayout(statsLayout);

    // 设置固定高度
    setFixedHeight(120);
    setMinimumWidth(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ChannelCardWidget::applyStyles() {
    // 卡片样式
    setStyleSheet(R"(
        #channelCard {
            background-color: white;
            border: 1px solid #E5E7EB;
            border-radius: 8px;
        }
        #channelCard:hover {
            border-color: #3B82F6;
            background-color: #F9FAFB;
        }
        #channelName {
            color: #111827;
        }
        #statusText {
            color: #6B7280;
        }
        #infoText {
            color: #9CA3AF;
        }
        #statLabel {
            color: #6B7280;
        }
        #iconButton {
            background-color: transparent;
            border: 1px solid #E5E7EB;
            border-radius: 4px;
            color: #6B7280;
        }
        #iconButton:hover {
            background-color: #F3F4F6;
            border-color: #3B82F6;
            color: #3B82F6;
        }
        #statusIndicator {
            border-radius: 6px;
        }
    )");

    // 阴影效果
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(8);
    shadow->setXOffset(0);
    shadow->setYOffset(2);
    shadow->setColor(QColor(0, 0, 0, 15));
    setGraphicsEffect(shadow);
}

void ChannelCardWidget::updateDisplay() {
    if (!m_channel) return;

    // 更新名称
    m_nameLabel->setText(m_channel->getName());

    // 更新状态
    ChannelState state = m_channel->getState();
    m_statusLabel->setText(getChannelStateText(state));

    QColor stateColor = getChannelStateColor(state);
    m_statusIndicator->setStyleSheet(QString(
        "background-color: %1; border-radius: 6px;"
    ).arg(stateColor.name()));

    // 更新开始/停止按钮
    if (state == ChannelState::Running) {
        m_startStopBtn->setText("⏸");
        m_startStopBtn->setToolTip(tr("停止通道"));
    } else {
        m_startStopBtn->setText("▶");
        m_startStopBtn->setToolTip(tr("启动通道"));
    }

    // 更新连接信息
    m_connectionLabel->setText(getConnectionInfo());

    // 更新统计
    int collectorCount = m_channel->getCollectors().size();
    int serverCount = m_channel->getServers().size();
    m_collectorCountLabel->setText(tr("📥 采集器: %1").arg(collectorCount));
    m_serverCountLabel->setText(tr("📤 服务器: %1").arg(serverCount));

    update();
}

void ChannelCardWidget::setSelected(bool selected) {
    if (m_selected != selected) {
        m_selected = selected;

        if (m_selected) {
            setStyleSheet(styleSheet() + R"(
                #channelCard {
                    border: 2px solid #3B82F6;
                    background-color: #EFF6FF;
                }
            )");
        } else {
            applyStyles();
        }

        update();
    }
}

void ChannelCardWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_channel);
    }
    QFrame::mousePressEvent(event);
}

void ChannelCardWidget::enterEvent(QEnterEvent* event) {
    m_hovered = true;
    update();
    QFrame::enterEvent(event);
}

void ChannelCardWidget::leaveEvent(QEvent* event) {
    m_hovered = false;
    update();
    QFrame::leaveEvent(event);
}

void ChannelCardWidget::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);
}

void ChannelCardWidget::onChannelStateChanged(ChannelState state) {
    updateDisplay();
}

void ChannelCardWidget::onStartStopClicked() {
    if (!m_channel) return;

    if (m_channel->getState() == ChannelState::Running) {
        emit stopRequested(m_channel);
    } else {
        emit startRequested(m_channel);
    }
}

void ChannelCardWidget::onEditClicked() {
    emit editRequested(m_channel);
}

void ChannelCardWidget::onDeleteClicked() {
    emit deleteRequested(m_channel);
}

void ChannelCardWidget::updateIndicatorAnimation() {
    if (!m_channel) return;

    ChannelState state = m_channel->getState();
    if (state == ChannelState::Starting || state == ChannelState::Stopping) {
        m_animationPhase = (m_animationPhase + 1) % 4;

        QColor color = getChannelStateColor(state);
        int alpha = 100 + (155 * qAbs(m_animationPhase - 2)) / 2;
        color.setAlpha(alpha);

        m_statusIndicator->setStyleSheet(QString(
            "background-color: %1; border-radius: 6px;"
        ).arg(color.name(QColor::HexArgb)));
    }
}

QString ChannelCardWidget::getChannelStateText(ChannelState state) const {
    switch (state) {
        case ChannelState::Stopped:
            return tr("已停止");
        case ChannelState::Starting:
            return tr("正在启动...");
        case ChannelState::Running:
            return tr("运行中");
        case ChannelState::Stopping:
            return tr("正在停止...");
        case ChannelState::Error:
            return tr("错误");
        default:
            return tr("未知");
    }
}

QColor ChannelCardWidget::getChannelStateColor(ChannelState state) const {
    switch (state) {
        case ChannelState::Stopped:
            return QColor("#9CA3AF");  // 灰色
        case ChannelState::Starting:
            return QColor("#F59E0B");  // 橙色
        case ChannelState::Running:
            return QColor("#10B981");  // 绿色
        case ChannelState::Stopping:
            return QColor("#F59E0B");  // 橙色
        case ChannelState::Error:
            return QColor("#EF4444");  // 红色
        default:
            return QColor("#6B7280");
    }
}

QString ChannelCardWidget::getConnectionInfo() const {
    if (!m_channel) return QString();

    int connectedCollectors = 0;
    int totalCollectors = m_channel->getCollectors().size();

    for (ICollector* collector : m_channel->getCollectors()) {
        if (collector->isConnected()) {
            connectedCollectors++;
        }
    }

    int activeServers = 0;
    int totalServers = m_channel->getServers().size();

    for (IServer* server : m_channel->getServers()) {
        if (server->isRunning()) {
            activeServers++;
        }
    }

    return tr("连接: %1/%2 | 服务: %3/%4")
        .arg(connectedCollectors)
        .arg(totalCollectors)
        .arg(activeServers)
        .arg(totalServers);
}

} // namespace ModbusPlexLink
