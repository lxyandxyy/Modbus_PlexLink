#include "DataMonitorWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QFileDialog>
#include <QTextStream>
#include <QClipboard>
#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <QDebug>
#include <QSignalBlocker>

namespace ModbusPlexLink {

DataMonitorWidget::DataMonitorWidget(ChannelManager* channelManager, QWidget *parent)
    : QWidget(parent)
    , m_channelManager(channelManager)
    , m_currentChannel(nullptr)
    , m_channelFilter(nullptr)
    , m_searchBox(nullptr)
    , m_clearSearchBtn(nullptr)
    , m_dataTable(nullptr)
    , m_refreshBtn(nullptr)
    , m_exportBtn(nullptr)
    , m_refreshIntervalCombo(nullptr)
    , m_statusLabel(nullptr)
    , m_refreshTimer(new QTimer(this))
    , m_autoRefresh(true)
    , m_channelFilterLocked(false)
    , m_selectedChannelFilter(tr("全部"))
{
    setupUi();

    // 连接信号
    connect(m_refreshTimer, &QTimer::timeout, this, &DataMonitorWidget::updateDataTable);
    m_refreshTimer->start(1000);  // 默认1秒刷新

    // 监听所有通道的数据更新
    if (m_channelManager) {
        for (const QString& channelName : m_channelManager->getChannelNames()) {
            Channel* channel = m_channelManager->getChannel(channelName);
            if (channel && channel->getDataModel()) {
                connect(channel->getDataModel(), &UniversalDataModel::dataUpdated,
                        this, &DataMonitorWidget::onDataUpdated);
            }
        }

        // 监听新通道创建
        connect(m_channelManager, &ChannelManager::channelCreated, this, [this](const QString& name) {
            Channel* channel = m_channelManager->getChannel(name);
            if (channel && channel->getDataModel()) {
                connect(channel->getDataModel(), &UniversalDataModel::dataUpdated,
                        this, &DataMonitorWidget::onDataUpdated);
            }
            // 更新通道过滤器
            m_channelFilter->clear();
            m_channelFilter->addItem(tr("全部"));
            m_channelFilter->addItems(m_channelManager->getChannelNames());
        });
    }

    refreshDisplay();
}

DataMonitorWidget::~DataMonitorWidget() {
}

void DataMonitorWidget::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 标题
    QLabel* titleLabel = new QLabel(tr("实时数据监控"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout* toolLayout = new QHBoxLayout();

    // 通道过滤
    toolLayout->addWidget(new QLabel(tr("通道:"), this));
    m_channelFilter = new QComboBox(this);
    m_channelFilter->addItem(tr("全部"));
    if (m_channelManager) {
        m_channelFilter->addItems(m_channelManager->getChannelNames());
    }
    connect(m_channelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataMonitorWidget::onChannelFilterChanged);
    toolLayout->addWidget(m_channelFilter);

    toolLayout->addSpacing(20);

    // 搜索框
    toolLayout->addWidget(new QLabel(tr("搜索:"), this));
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("输入标签名搜索..."));
    m_searchBox->setMinimumWidth(200);
    connect(m_searchBox, &QLineEdit::textChanged, this, &DataMonitorWidget::onSearchTextChanged);
    toolLayout->addWidget(m_searchBox);

    m_clearSearchBtn = new QPushButton(tr("清除"), this);
    connect(m_clearSearchBtn, &QPushButton::clicked, this, &DataMonitorWidget::onClearSearch);
    toolLayout->addWidget(m_clearSearchBtn);

    toolLayout->addSpacing(20);

    // 刷新控制
    toolLayout->addWidget(new QLabel(tr("刷新间隔:"), this));
    m_refreshIntervalCombo = new QComboBox(this);
    m_refreshIntervalCombo->addItems({"0.5秒", "1秒", "2秒", "5秒", "10秒"});
    m_refreshIntervalCombo->setCurrentIndex(1);  // 默认1秒
    connect(m_refreshIntervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataMonitorWidget::onRefreshIntervalChanged);
    toolLayout->addWidget(m_refreshIntervalCombo);

    m_refreshBtn = new QPushButton(tr("手动刷新"), this);
    connect(m_refreshBtn, &QPushButton::clicked, this, &DataMonitorWidget::onManualRefresh);
    toolLayout->addWidget(m_refreshBtn);

    m_exportBtn = new QPushButton(tr("导出CSV"), this);
    connect(m_exportBtn, &QPushButton::clicked, this, &DataMonitorWidget::onExportToCSV);
    toolLayout->addWidget(m_exportBtn);

    toolLayout->addStretch();
    mainLayout->addLayout(toolLayout);

    // 数据表格
    m_dataTable = new QTableWidget(this);
    m_dataTable->setColumnCount(6);
    m_dataTable->setHorizontalHeaderLabels({
        tr("通道"), tr("标签名"), tr("值"), tr("质量"), tr("时间戳"), tr("年龄")
    });

    // 表格样式
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dataTable->setAlternatingRowColors(true);
    m_dataTable->setSortingEnabled(true);
    m_dataTable->verticalHeader()->setVisible(false);
    m_dataTable->horizontalHeader()->setStretchLastSection(true);

    // 列宽
    m_dataTable->setColumnWidth(0, 120);  // 通道
    m_dataTable->setColumnWidth(1, 200);  // 标签名
    m_dataTable->setColumnWidth(2, 150);  // 值
    m_dataTable->setColumnWidth(3, 80);   // 质量
    m_dataTable->setColumnWidth(4, 180);  // 时间戳
    m_dataTable->setColumnWidth(5, 100);  // 年龄

    // 右键菜单
    m_dataTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_dataTable, &QTableWidget::customContextMenuRequested,
            this, &DataMonitorWidget::onTableContextMenu);

    mainLayout->addWidget(m_dataTable);

    // 状态栏
    m_statusLabel = new QLabel(tr("数据点: 0 | 已过滤: 0"), this);
    mainLayout->addWidget(m_statusLabel);
}

void DataMonitorWidget::setChannel(Channel* channel) {
    m_currentChannel = channel;
    if (!m_channelFilter) {
        refreshDisplay();
        return;
    }

    if (channel && m_channelManager) {
        QString channelName = channel->getName();
        QSignalBlocker blocker(m_channelFilter);
        int index = m_channelFilter->findText(channelName);
        if (index < 0) {
            m_channelFilter->addItem(channelName);
            index = m_channelFilter->findText(channelName);
        }
        if (index >= 0) {
            m_channelFilter->setCurrentIndex(index);
            m_selectedChannelFilter = channelName;
        }
        m_channelFilterLocked = true;
        m_channelFilter->setEnabled(false);
    } else {
        m_channelFilterLocked = false;
        m_channelFilter->setEnabled(true);
        if (m_channelFilter->count() > 0) {
            QSignalBlocker blocker(m_channelFilter);
            m_channelFilter->setCurrentIndex(0);
        }
        m_selectedChannelFilter = m_channelFilter->count() > 0
            ? m_channelFilter->currentText()
            : tr("全部");
    }

    refreshDisplay();
}

void DataMonitorWidget::refreshDisplay() {
    updateDataTable();
}

void DataMonitorWidget::onSearchTextChanged(const QString& text) {
    m_searchText = text.trimmed();
    updateDataTable();
}

void DataMonitorWidget::onChannelFilterChanged(int index) {
    m_selectedChannelFilter = m_channelFilter->currentText();
    updateDataTable();
}

void DataMonitorWidget::onClearSearch() {
    m_searchBox->clear();
    if (!m_channelFilterLocked) {
        m_channelFilter->setCurrentIndex(0);  // 选择"全部"
    } else {
        updateDataTable();
    }
}

void DataMonitorWidget::onDataUpdated(const QString& tagName, const DataPoint& point) {
    // 获取发送信号的通道
    UniversalDataModel* udm = qobject_cast<UniversalDataModel*>(sender());
    if (!udm || !m_channelManager) return;

    // 找到对应的通道
    QString channelName;
    for (const QString& name : m_channelManager->getChannelNames()) {
        Channel* channel = m_channelManager->getChannel(name);
        if (channel && channel->getDataModel() == udm) {
            channelName = name;
            break;
        }
    }

    if (channelName.isEmpty()) return;

    // 更新缓存
    QString key = channelName + ":" + tagName;
    DataEntry entry;
    entry.channelName = channelName;
    entry.tagName = tagName;
    entry.point = point;
    m_dataCache[key] = entry;

    // 如果匹配过滤条件，立即更新显示（提升实时性）
    if (matchesFilter(channelName, tagName)) {
        addOrUpdateRow(channelName, tagName, point);
    }
}

void DataMonitorWidget::onAutoRefreshToggled(bool checked) {
    m_autoRefresh = checked;
    if (m_autoRefresh) {
        m_refreshTimer->start();
    } else {
        m_refreshTimer->stop();
    }
}

void DataMonitorWidget::onRefreshIntervalChanged(int index) {
    const int intervals[] = {500, 1000, 2000, 5000, 10000};  // 毫秒
    if (index >= 0 && index < 5) {
        m_refreshTimer->setInterval(intervals[index]);
    }
}

void DataMonitorWidget::onManualRefresh() {
    updateDataTable();
}

void DataMonitorWidget::onTableContextMenu(const QPoint& pos) {
    QMenu menu(this);

    QAction* copyAction = menu.addAction(tr("复制值"));
    connect(copyAction, &QAction::triggered, this, &DataMonitorWidget::onCopySelectedValue);

    QAction* exportAction = menu.addAction(tr("导出所有数据到CSV"));
    connect(exportAction, &QAction::triggered, this, &DataMonitorWidget::onExportToCSV);

    menu.exec(m_dataTable->viewport()->mapToGlobal(pos));
}

void DataMonitorWidget::onCopySelectedValue() {
    int row = m_dataTable->currentRow();
    if (row < 0) return;

    QTableWidgetItem* valueItem = m_dataTable->item(row, 2);
    if (valueItem) {
        QApplication::clipboard()->setText(valueItem->text());
        m_statusLabel->setText(tr("已复制: %1").arg(valueItem->text()));
    }
}

void DataMonitorWidget::onExportToCSV() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("导出数据到CSV"),
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_monitor_data.csv",
        tr("CSV文件 (*.csv)"));

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法创建文件: %1").arg(fileName));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);  // Qt6 方式设置编码

    // 写入表头
    out << "通道,标签名,值,质量,时间戳,年龄(秒)\n";

    // 写入数据
    for (int row = 0; row < m_dataTable->rowCount(); ++row) {
        for (int col = 0; col < m_dataTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_dataTable->item(row, col);
            if (item) {
                out << item->text();
            }
            if (col < m_dataTable->columnCount() - 1) {
                out << ",";
            }
        }
        out << "\n";
    }

    file.close();

    QMessageBox::information(this, tr("导出成功"),
        tr("已导出 %1 条数据到:\n%2").arg(m_dataTable->rowCount()).arg(fileName));
}

void DataMonitorWidget::updateDataTable() {
    if (!m_channelManager) return;

    // 禁用排序以提升性能
    m_dataTable->setSortingEnabled(false);

    // 清空现有数据
    m_dataTable->setRowCount(0);
    m_dataCache.clear();

    int totalPoints = 0;
    int displayedPoints = 0;

    // 遍历所有通道
    for (const QString& channelName : m_channelManager->getChannelNames()) {
        Channel* channel = m_channelManager->getChannel(channelName);
        if (!channel || !channel->getDataModel()) continue;

        // 获取通道的所有数据点
        UniversalDataModel* udm = channel->getDataModel();
        QHash<QString, DataPoint> allPoints = udm->readAllPoints();
        totalPoints += allPoints.size();

        // 遍历数据点
        for (auto it = allPoints.constBegin(); it != allPoints.constEnd(); ++it) {
            const QString& tagName = it.key();
            const DataPoint& point = it.value();

            // 更新缓存
            QString key = channelName + ":" + tagName;
            DataEntry entry;
            entry.channelName = channelName;
            entry.tagName = tagName;
            entry.point = point;
            m_dataCache[key] = entry;

            // 应用过滤
            if (matchesFilter(channelName, tagName)) {
                addOrUpdateRow(channelName, tagName, point);
                displayedPoints++;
            }
        }
    }

    // 恢复排序
    m_dataTable->setSortingEnabled(true);

    // 更新状态
    m_statusLabel->setText(tr("数据点: %1 | 显示: %2")
        .arg(totalPoints).arg(displayedPoints));
}

void DataMonitorWidget::addOrUpdateRow(const QString& channelName,
                                        const QString& tagName,
                                        const DataPoint& point) {
    // 查找是否已存在
    int existingRow = -1;
    for (int row = 0; row < m_dataTable->rowCount(); ++row) {
        QTableWidgetItem* channelItem = m_dataTable->item(row, 0);
        QTableWidgetItem* tagItem = m_dataTable->item(row, 1);
        if (channelItem && tagItem &&
            channelItem->text() == channelName &&
            tagItem->text() == tagName) {
            existingRow = row;
            break;
        }
    }

    int row = existingRow;
    if (row < 0) {
        // 新增行
        row = m_dataTable->rowCount();
        m_dataTable->insertRow(row);

        // 设置通道名和标签名（不可编辑）
        QTableWidgetItem* channelItem = new QTableWidgetItem(channelName);
        channelItem->setFlags(channelItem->flags() & ~Qt::ItemIsEditable);
        m_dataTable->setItem(row, 0, channelItem);

        QTableWidgetItem* tagItem = new QTableWidgetItem(tagName);
        tagItem->setFlags(tagItem->flags() & ~Qt::ItemIsEditable);
        m_dataTable->setItem(row, 1, tagItem);
    }

    // 更新值
    QString valueStr;
    if (point.value.type() == QVariant::Double) {
        valueStr = QString::number(point.value.toDouble(), 'f', 3);
    } else {
        valueStr = point.value.toString();
    }
    QTableWidgetItem* valueItem = new QTableWidgetItem(valueStr);
    m_dataTable->setItem(row, 2, valueItem);

    // 更新质量
    QString qualityStr = qualityToString(point.quality);
    QTableWidgetItem* qualityItem = new QTableWidgetItem(qualityStr);
    qualityItem->setBackground(qualityColor(point.quality));
    m_dataTable->setItem(row, 3, qualityItem);

    // 更新时间戳
    QString timestampStr = formatTimestamp(point.timestamp);
    QTableWidgetItem* timestampItem = new QTableWidgetItem(timestampStr);
    m_dataTable->setItem(row, 4, timestampItem);

    // 计算数据年龄（秒）
    qint64 age = (QDateTime::currentMSecsSinceEpoch() - point.timestamp) / 1000;
    QTableWidgetItem* ageItem = new QTableWidgetItem(QString::number(age));
    if (age > 10) {
        ageItem->setBackground(QColor(255, 200, 200));  // 超过10秒标红
    }
    m_dataTable->setItem(row, 5, ageItem);
}

QString DataMonitorWidget::qualityToString(DataQuality quality) const {
    switch (quality) {
        case DataQuality::Good: return tr("良好");
        case DataQuality::Bad: return tr("异常");
        case DataQuality::Uncertain: return tr("不确定");
        case DataQuality::NotUpdated: return tr("未更新");
        default: return tr("未知");
    }
}

QColor DataMonitorWidget::qualityColor(DataQuality quality) const {
    switch (quality) {
        case DataQuality::Good: return QColor(200, 255, 200);       // 浅绿
        case DataQuality::Bad: return QColor(255, 200, 200);        // 浅红
        case DataQuality::Uncertain: return QColor(255, 255, 200);  // 浅黄
        case DataQuality::NotUpdated: return QColor(220, 220, 220); // 灰色
        default: return Qt::white;
    }
}

QString DataMonitorWidget::formatTimestamp(qint64 timestamp) const {
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
    return dt.toString("yyyy-MM-dd HH:mm:ss.zzz");
}

bool DataMonitorWidget::matchesFilter(const QString& channelName, const QString& tagName) const {
    // 通道过滤
    if (m_selectedChannelFilter != tr("全部") && channelName != m_selectedChannelFilter) {
        return false;
    }

    // 文本搜索（不区分大小写）
    if (!m_searchText.isEmpty()) {
        if (!tagName.contains(m_searchText, Qt::CaseInsensitive)) {
            return false;
        }
    }

    return true;
}

} // namespace ModbusPlexLink
