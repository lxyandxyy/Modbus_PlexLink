#include "DashboardDataWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDateTime>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QComboBox>

namespace ModbusPlexLink {

// ============================================================================
// DataPointCard 实现
// ============================================================================

DataPointCard::DataPointCard(const QString& tagName, const DataPoint& point, QWidget *parent)
    : QFrame(parent)
    , m_tagName(tagName)
    , m_dataPoint(point)
    , m_isRemote(false)
    , m_isOffline(false)
    , m_remoteIndicator(nullptr)
    , m_offlineOverlay(nullptr)
{
    setObjectName("dataPointCard");
    setupUi();
    updateData(point);
}

void DataPointCard::setupUi() {
    setFixedSize(180, 140);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    // 标签名行（包含远程指示器）
    QHBoxLayout* nameRow = new QHBoxLayout();
    nameRow->setSpacing(4);
    
    m_tagLabel = new QLabel(m_tagName, this);
    m_tagLabel->setObjectName("tagLabel");
    m_tagLabel->setWordWrap(true);
    m_tagLabel->setMaximumHeight(32);
    QFont tagFont = m_tagLabel->font();
    tagFont.setPointSize(9);
    tagFont.setBold(true);
    m_tagLabel->setFont(tagFont);
    nameRow->addWidget(m_tagLabel, 1);
    
    // 远程指示器（默认隐藏）
    m_remoteIndicator = new QLabel("🌐", this);
    m_remoteIndicator->setToolTip(tr("远程数据"));
    m_remoteIndicator->setFixedSize(16, 16);
    m_remoteIndicator->hide();
    nameRow->addWidget(m_remoteIndicator);
    
    layout->addLayout(nameRow);

    // 质量指示器
    m_qualityIndicator = new QLabel(this);
    m_qualityIndicator->setFixedSize(8, 8);
    m_qualityIndicator->setStyleSheet("border-radius: 4px;");

    QHBoxLayout* qualityLayout = new QHBoxLayout();
    qualityLayout->addWidget(m_qualityIndicator);
    qualityLayout->addStretch();
    layout->addLayout(qualityLayout);

    layout->addStretch();

    // 数值
    m_valueLabel = new QLabel(this);
    m_valueLabel->setObjectName("valueLabel");
    m_valueLabel->setAlignment(Qt::AlignCenter);
    QFont valueFont = m_valueLabel->font();
    valueFont.setPointSize(16);
    valueFont.setBold(true);
    m_valueLabel->setFont(valueFont);
    layout->addWidget(m_valueLabel);

    // 单位（预留）
    m_unitLabel = new QLabel(this);
    m_unitLabel->setObjectName("unitLabel");
    m_unitLabel->setAlignment(Qt::AlignCenter);
    QFont unitFont = m_unitLabel->font();
    unitFont.setPointSize(8);
    m_unitLabel->setFont(unitFont);
    layout->addWidget(m_unitLabel);

    layout->addStretch();

    // 时间戳
    m_timestampLabel = new QLabel(this);
    m_timestampLabel->setObjectName("timestampLabel");
    m_timestampLabel->setAlignment(Qt::AlignRight);
    QFont timeFont = m_timestampLabel->font();
    timeFont.setPointSize(7);
    m_timestampLabel->setFont(timeFont);
    layout->addWidget(m_timestampLabel);

    // 样式
    setStyleSheet(R"(
        #dataPointCard {
            background-color: white;
            border: 1px solid #E5E7EB;
            border-radius: 8px;
        }
        #dataPointCard:hover {
            border-color: #3B82F6;
        }
        #tagLabel {
            color: #374151;
        }
        #valueLabel {
            color: #111827;
        }
        #unitLabel {
            color: #6B7280;
        }
        #timestampLabel {
            color: #9CA3AF;
        }
    )");

    // 阴影
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(6);
    shadow->setXOffset(0);
    shadow->setYOffset(2);
    shadow->setColor(QColor(0, 0, 0, 10));
    setGraphicsEffect(shadow);
}

void DataPointCard::updateData(const DataPoint& point) {
    m_dataPoint = point;

    // 更新数值
    m_valueLabel->setText(formatValue(point.value));

    // 更新质量指示器
    QColor qualityColor = getQualityColor(point.quality);
    m_qualityIndicator->setStyleSheet(QString(
        "background-color: %1; border-radius: 4px;"
    ).arg(qualityColor.name()));

    // 更新时间戳
    if (point.timestamp > 0) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(point.timestamp);
        m_timestampLabel->setText(dt.toString("hh:mm:ss"));
    } else {
        m_timestampLabel->setText("--:--:--");
    }
}

void DataPointCard::updateData(const QJsonObject& pointJson) {
    // 从JSON更新数据（用于远程数据）
    QVariant value = pointJson["value"].toVariant();
    m_valueLabel->setText(formatValue(value));
    
    // 质量状态
    QString qualityStr = pointJson["quality"].toString("Good");
    DataQuality quality = DataQuality::Good;
    if (qualityStr == "Bad") quality = DataQuality::Bad;
    else if (qualityStr == "Uncertain") quality = DataQuality::Uncertain;
    else if (qualityStr == "NotUpdated") quality = DataQuality::NotUpdated;
    
    QColor qualityColor = getQualityColor(quality);
    m_qualityIndicator->setStyleSheet(QString(
        "background-color: %1; border-radius: 4px;"
    ).arg(qualityColor.name()));
    
    // 时间戳
    QString timestamp = pointJson["timestamp"].toString();
    if (!timestamp.isEmpty()) {
        QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
        if (dt.isValid()) {
            m_timestampLabel->setText(dt.toString("hh:mm:ss"));
        }
    }
}

void DataPointCard::setRemoteMode(bool remote) {
    m_isRemote = remote;
    if (m_remoteIndicator) {
        m_remoteIndicator->setVisible(remote);
    }
    
    // 远程模式使用不同的边框颜色
    if (remote) {
        setStyleSheet(R"(
            #dataPointCard {
                background-color: #FFFBEB;
                border: 1px solid #F59E0B;
                border-radius: 8px;
            }
            #dataPointCard:hover {
                border-color: #D97706;
            }
            #tagLabel { color: #92400E; }
            #valueLabel { color: #78350F; }
            #unitLabel { color: #A16207; }
            #timestampLabel { color: #CA8A04; }
        )");
    } else {
        setStyleSheet(R"(
            #dataPointCard {
                background-color: white;
                border: 1px solid #E5E7EB;
                border-radius: 8px;
            }
            #dataPointCard:hover {
                border-color: #3B82F6;
            }
            #tagLabel { color: #374151; }
            #valueLabel { color: #111827; }
            #unitLabel { color: #6B7280; }
            #timestampLabel { color: #9CA3AF; }
        )");
    }
}

void DataPointCard::setOffline(bool offline) {
    m_isOffline = offline;
    
    if (!m_offlineOverlay) {
        // 创建离线遮罩层
        m_offlineOverlay = new QLabel(this);
        m_offlineOverlay->setText("⚠️ 离线");
        m_offlineOverlay->setAlignment(Qt::AlignCenter);
        m_offlineOverlay->setStyleSheet(R"(
            QLabel {
                background-color: rgba(239, 68, 68, 0.85);
                color: white;
                font-size: 12px;
                font-weight: bold;
                border-radius: 6px;
                padding: 4px 8px;
            }
        )");
        m_offlineOverlay->setFixedSize(80, 28);
        // 居中显示
        m_offlineOverlay->move((width() - 80) / 2, (height() - 28) / 2);
    }
    
    m_offlineOverlay->setVisible(offline);
    
    // 离线时调整卡片透明度效果
    if (offline) {
        setStyleSheet(R"(
            #dataPointCard {
                background-color: #F3F4F6;
                border: 1px solid #D1D5DB;
                border-radius: 8px;
            }
            #tagLabel { color: #9CA3AF; }
            #valueLabel { color: #9CA3AF; }
            #unitLabel { color: #D1D5DB; }
            #timestampLabel { color: #D1D5DB; }
        )");
    } else {
        // 恢复正常样式
        if (m_isRemote) {
            setRemoteMode(true);
        } else {
            setRemoteMode(false);
        }
    }
}

QString DataPointCard::formatValue(const QVariant& value) const {
    if (!value.isValid() || value.isNull()) {
        return "--";
    }

    bool ok;
    double num = value.toDouble(&ok);
    if (ok) {
        // 格式化数字
        // 0或接近0的值直接显示为0
        if (qAbs(num) < 1e-10) {
            return "0";
        }
        // 非常小或非常大的数使用科学计数法
        if (qAbs(num) < 0.001 || qAbs(num) > 999999) {
            return QString::number(num, 'e', 2);
        }
        // 普通数字使用定点格式
        if (qAbs(num) < 1) {
            return QString::number(num, 'f', 4);  // 小数保留4位
        } else if (qAbs(num) < 100) {
            return QString::number(num, 'f', 2);  // 正常数保留2位
        } else {
            return QString::number(num, 'f', 1);  // 大数保留1位
        }
    }

    return value.toString();
}

QColor DataPointCard::getQualityColor(DataQuality quality) const {
    switch (quality) {
        case DataQuality::Good:
            return QColor("#10B981");  // 绿色
        case DataQuality::Bad:
            return QColor("#EF4444");  // 红色
        case DataQuality::Uncertain:
            return QColor("#F59E0B");  // 橙色
        case DataQuality::NotUpdated:
            return QColor("#9CA3AF");  // 灰色
        default:
            return QColor("#6B7280");
    }
}

// ============================================================================
// DashboardDataWidget 实现
// ============================================================================

DashboardDataWidget::DashboardDataWidget(QWidget *parent)
    : QWidget(parent)
    , m_channel(nullptr)
    , m_isRemoteMode(false)
    , m_headerWidget(nullptr)
    , m_channelNameLabel(nullptr)
    , m_channelStatusLabel(nullptr)
    , m_channelOnline(false)
{
    setupUi();

    // 自动刷新定时器
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout,
            this, &DashboardDataWidget::onAutoRefreshTimeout);
    m_refreshTimer->start(2000);  // 每2秒刷新
}

DashboardDataWidget::~DashboardDataWidget() {
}

void DashboardDataWidget::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 头部区域 - 显示当前通道名称和状态
    m_headerWidget = new QWidget(this);
    m_headerWidget->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border-bottom: 1px solid #E5E7EB;
        }
    )");
    QHBoxLayout* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(16, 10, 16, 10);
    headerLayout->setSpacing(12);
    
    m_channelNameLabel = new QLabel(tr("📊 实时数据"), m_headerWidget);
    m_channelNameLabel->setStyleSheet(R"(
        QLabel {
            font-size: 14px;
            font-weight: bold;
            color: #1F2937;
            border: none;
            background: transparent;
        }
    )");
    headerLayout->addWidget(m_channelNameLabel);
    
    m_channelStatusLabel = new QLabel(m_headerWidget);
    m_channelStatusLabel->setStyleSheet(R"(
        QLabel {
            font-size: 11px;
            font-weight: bold;
            color: #6B7280;
            padding: 3px 10px;
            border-radius: 10px;
            background-color: #F3F4F6;
            border: none;
        }
    )");
    m_channelStatusLabel->hide();  // 初始隐藏
    headerLayout->addWidget(m_channelStatusLabel);
    
    headerLayout->addStretch();
    mainLayout->addWidget(m_headerWidget);

    // 滚动区域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 内容容器
    m_contentWidget = new QWidget();
    m_gridLayout = new QGridLayout(m_contentWidget);
    m_gridLayout->setContentsMargins(16, 16, 16, 16);
    m_gridLayout->setSpacing(12);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea);

    // 空状态标签
    m_emptyLabel = new QLabel(tr("🔍 暂无数据\n\n请选择一个运行中的通道"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(R"(
        QLabel {
            color: #9CA3AF;
            font-size: 14px;
            padding: 40px;
        }
    )");
    m_gridLayout->addWidget(m_emptyLabel, 0, 0, Qt::AlignCenter);

    // 滚动区域样式
    m_scrollArea->setStyleSheet(R"(
        QScrollArea {
            background-color: #F9FAFB;
            border: none;
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
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            border: none;
            background: #F3F4F6;
            height: 10px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #D1D5DB;
            min-width: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #9CA3AF;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
    )");
}

void DashboardDataWidget::setChannel(Channel* channel) {
    // 退出远程模式
    m_isRemoteMode = false;
    m_remoteChannelName.clear();
    
    // 断开旧通道
    if (m_channel) {
        disconnect(m_channel->getDataModel(), nullptr, this, nullptr);
        disconnect(m_channel, nullptr, this, nullptr);
    }

    m_channel = channel;
    clear();

    // 连接新通道
    if (m_channel) {
        m_emptyLabel->hide();
        
        // 先断开可能存在的旧连接
        disconnect(m_channel, &Channel::stateChanged, this, nullptr);
        
        // 监听通道状态变化
        connect(m_channel, &Channel::stateChanged, this, [this]() {
            updateHeader();
            // 更新所有卡片的离线状态
            bool offline = !m_channel || !m_channel->isRunning();
            for (DataPointCard* card : m_cards) {
                card->setOffline(offline);
            }
        });

        UniversalDataModel* udm = m_channel->getDataModel();
        if (udm) {
            disconnect(udm, &UniversalDataModel::dataUpdated, this, nullptr);
            connect(udm, &UniversalDataModel::dataUpdated,
                    this, &DashboardDataWidget::onDataUpdated);
        }

        // 立即刷新显示
        refreshDisplay();
    } else {
        m_emptyLabel->show();
    }
    
    // 更新头部显示
    updateHeader();
}

void DashboardDataWidget::setRemoteMode(bool remote, const QString& channelName) {
    m_isRemoteMode = remote;
    m_remoteChannelName = channelName;
    
    if (remote) {
        // 断开本地通道
        if (m_channel) {
            disconnect(m_channel->getDataModel(), nullptr, this, nullptr);
            disconnect(m_channel, nullptr, this, nullptr);
        }
        m_channel = nullptr;
        m_channelOnline = false;  // 远程通道状态未知
        clear();
        
        if (channelName.isEmpty()) {
            showRemoteModeHint(tr("🌐 远程模式 - 请在左侧选择一个远程通道"));
        } else {
            // 不显示提示，让头部显示通道名称和状态
            m_emptyLabel->hide();
        }
    } else {
        // 退出远程模式
        clear();
        m_emptyLabel->setText(tr("🔍 暂无数据\n\n请选择一个运行中的通道"));
        m_emptyLabel->setStyleSheet(R"(
            QLabel {
                color: #9CA3AF;
                font-size: 14px;
                padding: 40px;
            }
        )");
        m_emptyLabel->show();
    }
    
    // 更新头部显示
    updateHeader();
}

void DashboardDataWidget::updateRemoteData(const QString& channelName, const QJsonObject& data) {
    if (!m_isRemoteMode) return;
    
    // 检查是否是当前选中的通道
    if (!m_remoteChannelName.isEmpty() && m_remoteChannelName != channelName) {
        return;
    }
    
    m_remoteChannelName = channelName;
    m_emptyLabel->hide();
    
    // 标记通道在线（有数据更新）
    m_channelOnline = true;
    updateHeader();
    
    // 更新或创建数据卡片
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        QString tagName = it.key();
        QJsonObject pointJson = it.value().toObject();
        addOrUpdateRemoteCard(tagName, pointJson);
    }
}

void DashboardDataWidget::showRemoteModeHint(const QString& hint) {
    clear();
    m_emptyLabel->setText(hint);
    m_emptyLabel->setStyleSheet(R"(
        QLabel {
            color: #92400E;
            font-size: 14px;
            padding: 40px;
            background-color: #FFFBEB;
            border: 1px dashed #F59E0B;
            border-radius: 8px;
        }
    )");
    m_emptyLabel->show();
}

void DashboardDataWidget::addOrUpdateRemoteCard(const QString& tagName, const QJsonObject& pointJson) {
    if (m_cards.contains(tagName)) {
        // 更新现有卡片
        m_cards[tagName]->updateData(pointJson);
    } else {
        // 创建新卡片
        DataPoint emptyPoint;
        DataPointCard* card = new DataPointCard(tagName, emptyPoint, m_contentWidget);
        card->setRemoteMode(true);
        card->updateData(pointJson);
        m_cards[tagName] = card;

        // 计算网格位置 (每行4个卡片)
        int index = m_cards.size() - 1;
        int row = index / 4;
        int col = index % 4;
        m_gridLayout->addWidget(card, row, col);

        m_emptyLabel->hide();
    }
}

void DashboardDataWidget::refreshDisplay() {
    if (!m_channel) return;

    UniversalDataModel* udm = m_channel->getDataModel();
    if (!udm) return;

    // 获取所有数据点
    QHash<QString, DataPoint> allPoints = udm->readAllPoints();

    if (allPoints.isEmpty()) {
        if (m_cards.isEmpty()) {
            m_emptyLabel->setText(tr("⏳ 等待数据采集...\n\n通道正在运行，尚未收到数据"));
            m_emptyLabel->show();
        }
        return;
    }

    m_emptyLabel->hide();

    // 更新或添加卡片
    for (auto it = allPoints.constBegin(); it != allPoints.constEnd(); ++it) {
        addOrUpdateCard(it.key(), it.value());
    }
}

void DashboardDataWidget::clear() {
    // 清除所有卡片
    qDeleteAll(m_cards);
    m_cards.clear();

    m_emptyLabel->setText(tr("🔍 暂无数据\n\n请选择一个运行中的通道"));
    m_emptyLabel->show();
}

void DashboardDataWidget::onDataUpdated(const QString& tagName, const DataPoint& point) {
    addOrUpdateCard(tagName, point);
}

void DashboardDataWidget::onAutoRefreshTimeout() {
    // 定期刷新显示（更新时间戳等）
    for (auto card : m_cards) {
        card->update();
    }
}

void DashboardDataWidget::addOrUpdateCard(const QString& tagName, const DataPoint& point) {
    if (m_cards.contains(tagName)) {
        // 更新现有卡片
        m_cards[tagName]->updateData(point);
    } else {
        // 创建新卡片
        DataPointCard* card = new DataPointCard(tagName, point, m_contentWidget);
        m_cards[tagName] = card;

        // 计算网格位置 (每行4个卡片)
        int index = m_cards.size() - 1;
        int row = index / 4;
        int col = index % 4;
        m_gridLayout->addWidget(card, row, col);

        m_emptyLabel->hide();
    }
}

void DashboardDataWidget::rebuildCards() {
    // 暂时不需要实现，卡片是动态添加的
}

void DashboardDataWidget::updateHeader() {
    if (!m_channelNameLabel || !m_channelStatusLabel) return;
    
    if (m_isRemoteMode) {
        // 远程模式
        if (m_remoteChannelName.isEmpty()) {
            m_channelNameLabel->setText(tr("📊 实时数据 - 远程模式，请选择通道"));
            m_channelStatusLabel->hide();
        } else {
            m_channelNameLabel->setText(tr("📊 实时数据 - 远程通道: %1").arg(m_remoteChannelName));
            m_channelStatusLabel->setText(m_channelOnline ? tr("🟢 在线") : tr("⚪ 等待数据"));
            m_channelStatusLabel->setStyleSheet(m_channelOnline ? R"(
                QLabel {
                    font-size: 11px;
                    font-weight: bold;
                    color: white;
                    padding: 3px 10px;
                    border-radius: 10px;
                    background-color: #10B981;
                    border: none;
                }
            )" : R"(
                QLabel {
                    font-size: 11px;
                    font-weight: bold;
                    color: #6B7280;
                    padding: 3px 10px;
                    border-radius: 10px;
                    background-color: #F3F4F6;
                    border: none;
                }
            )");
            m_channelStatusLabel->show();
        }
        
        // 远程模式头部背景色
        if (m_headerWidget) {
            m_headerWidget->setStyleSheet(R"(
                QWidget {
                    background-color: #FFFBEB;
                    border-bottom: 1px solid #F59E0B;
                }
            )");
        }
    } else if (m_channel) {
        // 本地模式 - 有通道
        m_channelNameLabel->setText(tr("📊 实时数据 - 本地通道: %1").arg(m_channel->getName()));
        
        bool isRunning = m_channel->isRunning();
        m_channelOnline = isRunning;
        
        if (isRunning) {
            m_channelStatusLabel->setText(tr("🟢 运行中"));
            m_channelStatusLabel->setStyleSheet(R"(
                QLabel {
                    font-size: 11px;
                    font-weight: bold;
                    color: white;
                    padding: 3px 10px;
                    border-radius: 10px;
                    background-color: #10B981;
                    border: none;
                }
            )");
        } else {
            m_channelStatusLabel->setText(tr("⚫ 已停止"));
            m_channelStatusLabel->setStyleSheet(R"(
                QLabel {
                    font-size: 11px;
                    font-weight: bold;
                    color: white;
                    padding: 3px 10px;
                    border-radius: 10px;
                    background-color: #EF4444;
                    border: none;
                }
            )");
        }
        m_channelStatusLabel->show();
        
        // 本地模式头部背景色
        if (m_headerWidget) {
            m_headerWidget->setStyleSheet(R"(
                QWidget {
                    background-color: white;
                    border-bottom: 1px solid #E5E7EB;
                }
            )");
        }
    } else {
        // 本地模式 - 无通道
        m_channelNameLabel->setText(tr("📊 实时数据"));
        m_channelStatusLabel->hide();
        
        if (m_headerWidget) {
            m_headerWidget->setStyleSheet(R"(
                QWidget {
                    background-color: white;
                    border-bottom: 1px solid #E5E7EB;
                }
            )");
        }
    }
}

} // namespace ModbusPlexLink
