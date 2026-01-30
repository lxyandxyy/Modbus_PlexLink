#include "SystemVariableManagerDialog.h"
#include "utils/SystemVariableManager.h"
#include "core/ChannelManager.h"
#include "core/DataTypes.h"
#include "DialogStyles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QDebug>

namespace ModbusPlexLink {

SystemVariableManagerDialog::SystemVariableManagerDialog(ChannelManager* channelManager,
                                                         QWidget* parent)
    : QDialog(parent)
    , m_channelManager(channelManager)
    , m_variableTable(nullptr)
    , m_channelFilter(nullptr)
    , m_collectorFilter(nullptr)
    , m_searchEdit(nullptr)
    , m_refreshBtn(nullptr)
    , m_exportBtn(nullptr)
    , m_copyBtn(nullptr)
    , m_countLabel(nullptr)
{
    setWindowTitle(tr("🔧 系统变量管理"));
    resize(900, 600);
    setMinimumSize(700, 400);
    
    setStyleSheet(DialogStyles::getDialogStyle());
    
    setupUi();
    
    // 同步变量
    SystemVariableManager::instance().syncFromChannels(m_channelManager);
    
    updateFilterOptions();
    refreshTable();
}

void SystemVariableManagerDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);
    
    // 顶部标题
    QWidget* headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 10);
    
    QLabel* iconLabel = new QLabel("📊", headerWidget);
    iconLabel->setStyleSheet("font-size: 28px;");
    headerLayout->addWidget(iconLabel);
    
    QLabel* titleLabel = new QLabel(tr("系统变量管理"), headerWidget);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #1E293B;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    
    QLabel* subtitleLabel = new QLabel(tr("查看所有采集通道的变量，用于服务通道映射"), headerWidget);
    subtitleLabel->setStyleSheet("color: #64748B; font-size: 11px;");
    headerLayout->addWidget(subtitleLabel);
    
    mainLayout->addWidget(headerWidget);
    
    // 筛选和搜索区域
    QWidget* filterCard = new QWidget(this);
    filterCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    QHBoxLayout* filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(16, 12, 16, 12);
    filterLayout->setSpacing(16);
    
    // 通道筛选
    QLabel* channelLabel = new QLabel(tr("通道:"), filterCard);
    channelLabel->setStyleSheet("border: none; background: transparent;");
    filterLayout->addWidget(channelLabel);
    
    m_channelFilter = new QComboBox(filterCard);
    m_channelFilter->setMinimumWidth(150);
    m_channelFilter->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px;");
    connect(m_channelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SystemVariableManagerDialog::onFilterChanged);
    filterLayout->addWidget(m_channelFilter);
    
    // 采集器筛选
    QLabel* collectorLabel = new QLabel(tr("采集器:"), filterCard);
    collectorLabel->setStyleSheet("border: none; background: transparent;");
    filterLayout->addWidget(collectorLabel);
    
    m_collectorFilter = new QComboBox(filterCard);
    m_collectorFilter->setMinimumWidth(150);
    m_collectorFilter->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px;");
    connect(m_collectorFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SystemVariableManagerDialog::onFilterChanged);
    filterLayout->addWidget(m_collectorFilter);
    
    filterLayout->addSpacing(20);
    
    // 搜索框
    QLabel* searchLabel = new QLabel(tr("🔍"), filterCard);
    searchLabel->setStyleSheet("border: none; background: transparent;");
    filterLayout->addWidget(searchLabel);
    
    m_searchEdit = new QLineEdit(filterCard);
    m_searchEdit->setPlaceholderText(tr("搜索变量名、注释..."));
    m_searchEdit->setMinimumWidth(200);
    m_searchEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px;");
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &SystemVariableManagerDialog::onSearchTextChanged);
    filterLayout->addWidget(m_searchEdit, 1);
    
    mainLayout->addWidget(filterCard);
    
    // 变量表格
    QWidget* tableCard = new QWidget(this);
    tableCard->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    QVBoxLayout* tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(16, 16, 16, 16);
    
    m_variableTable = new QTableWidget(tableCard);
    m_variableTable->setColumnCount(7);
    m_variableTable->setHorizontalHeaderLabels({
        tr("变量名"), tr("通道"), tr("采集器"), tr("数据类型"),
        tr("寄存器类型"), tr("注释"), tr("完整标识")
    });
    
    m_variableTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_variableTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_variableTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_variableTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_variableTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_variableTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_variableTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);
    
    m_variableTable->setColumnWidth(0, 150);
    m_variableTable->setColumnWidth(1, 120);
    m_variableTable->setColumnWidth(2, 120);
    m_variableTable->setColumnWidth(3, 80);
    m_variableTable->setColumnWidth(4, 100);
    m_variableTable->setColumnWidth(6, 250);
    
    m_variableTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_variableTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_variableTable->setAlternatingRowColors(true);
    m_variableTable->verticalHeader()->setVisible(false);
    m_variableTable->setStyleSheet("border: none;");
    
    tableCardLayout->addWidget(m_variableTable);
    
    mainLayout->addWidget(tableCard, 1);
    
    // 底部按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    
    m_countLabel = new QLabel(tr("共 0 个变量"), this);
    m_countLabel->setStyleSheet("color: #64748B;");
    buttonLayout->addWidget(m_countLabel);
    
    buttonLayout->addStretch();
    
    m_refreshBtn = new QPushButton(tr("🔄 刷新"), this);
    m_refreshBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #F1F5F9;
            border: 1px solid #E2E8F0;
            border-radius: 8px;
            padding: 8px 16px;
            color: #475569;
        }
        QPushButton:hover {
            background-color: #E2E8F0;
        }
    )");
    connect(m_refreshBtn, &QPushButton::clicked, this, &SystemVariableManagerDialog::onRefresh);
    buttonLayout->addWidget(m_refreshBtn);
    
    m_copyBtn = new QPushButton(tr("📋 复制标识"), this);
    m_copyBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #F1F5F9;
            border: 1px solid #E2E8F0;
            border-radius: 8px;
            padding: 8px 16px;
            color: #475569;
        }
        QPushButton:hover {
            background-color: #E2E8F0;
        }
    )");
    connect(m_copyBtn, &QPushButton::clicked, this, &SystemVariableManagerDialog::onCopyFullId);
    buttonLayout->addWidget(m_copyBtn);
    
    m_exportBtn = new QPushButton(tr("📥 导出CSV"), this);
    m_exportBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3B82F6;
            border: none;
            border-radius: 8px;
            padding: 8px 16px;
            color: white;
        }
        QPushButton:hover {
            background-color: #2563EB;
        }
    )");
    connect(m_exportBtn, &QPushButton::clicked, this, &SystemVariableManagerDialog::onExportCsv);
    buttonLayout->addWidget(m_exportBtn);
    
    QPushButton* closeBtn = new QPushButton(tr("关闭"), this);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #F1F5F9;
            border: 1px solid #E2E8F0;
            border-radius: 8px;
            padding: 8px 16px;
            color: #475569;
        }
        QPushButton:hover {
            background-color: #E2E8F0;
        }
    )");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);
    
    mainLayout->addLayout(buttonLayout);
}

void SystemVariableManagerDialog::onRefresh() {
    SystemVariableManager::instance().syncFromChannels(m_channelManager);
    updateFilterOptions();
    refreshTable();
}

void SystemVariableManagerDialog::onExportCsv() {
    QString filename = QFileDialog::getSaveFileName(
        this, tr("导出系统变量"), QString(), tr("CSV文件 (*.csv)"));
    
    if (filename.isEmpty()) return;
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法创建文件: %1").arg(filename));
        return;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入BOM
    out << "\xEF\xBB\xBF";
    
    // 写入表头
    out << "变量名,通道,采集器,数据类型,寄存器类型,注释,完整标识\n";
    
    // 写入数据
    for (int row = 0; row < m_variableTable->rowCount(); ++row) {
        QStringList fields;
        for (int col = 0; col < m_variableTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_variableTable->item(row, col);
            QString text = item ? item->text() : "";
            // 处理包含逗号或引号的字段
            if (text.contains(',') || text.contains('"') || text.contains('\n')) {
                text = "\"" + text.replace("\"", "\"\"") + "\"";
            }
            fields.append(text);
        }
        out << fields.join(",") << "\n";
    }
    
    file.close();
    
    QMessageBox::information(this, tr("导出成功"),
        tr("已导出 %1 个变量到文件:\n%2").arg(m_variableTable->rowCount()).arg(filename));
}

void SystemVariableManagerDialog::onFilterChanged() {
    // 更新采集器筛选选项
    QString selectedChannel = m_channelFilter->currentData().toString();
    
    m_collectorFilter->blockSignals(true);
    m_collectorFilter->clear();
    m_collectorFilter->addItem(tr("全部"), "");
    
    if (!selectedChannel.isEmpty()) {
        QStringList collectors = SystemVariableManager::instance().getCollectorNames(selectedChannel);
        for (const QString& collector : collectors) {
            m_collectorFilter->addItem(collector, collector);
        }
    }
    
    m_collectorFilter->blockSignals(false);
    
    refreshTable();
}

void SystemVariableManagerDialog::onSearchTextChanged(const QString& text) {
    Q_UNUSED(text);
    refreshTable();
}

void SystemVariableManagerDialog::onCopyFullId() {
    int row = m_variableTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个变量"));
        return;
    }
    
    QTableWidgetItem* item = m_variableTable->item(row, 6); // 完整标识列
    if (item) {
        QApplication::clipboard()->setText(item->text());
        QMessageBox::information(this, tr("已复制"),
            tr("已复制到剪贴板:\n%1").arg(item->text()));
    }
}

void SystemVariableManagerDialog::refreshTable() {
    m_variableTable->setRowCount(0);
    
    QString channelFilter = m_channelFilter->currentData().toString();
    QString collectorFilter = m_collectorFilter->currentData().toString();
    QString searchText = m_searchEdit->text().trimmed().toLower();
    
    QList<SystemVariable> variables;
    
    if (!channelFilter.isEmpty() && !collectorFilter.isEmpty()) {
        variables = SystemVariableManager::instance().getVariablesByCollector(channelFilter, collectorFilter);
    } else if (!channelFilter.isEmpty()) {
        variables = SystemVariableManager::instance().getVariablesByChannel(channelFilter);
    } else {
        variables = SystemVariableManager::instance().getAllVariables();
    }
    
    // 应用搜索过滤
    if (!searchText.isEmpty()) {
        QList<SystemVariable> filtered;
        for (const SystemVariable& var : variables) {
            if (var.variableName.toLower().contains(searchText) ||
                var.comment.toLower().contains(searchText) ||
                var.fullId.toLower().contains(searchText)) {
                filtered.append(var);
            }
        }
        variables = filtered;
    }
    
    // 填充表格
    m_variableTable->setRowCount(variables.size());
    
    for (int i = 0; i < variables.size(); ++i) {
        const SystemVariable& var = variables[i];
        
        m_variableTable->setItem(i, 0, new QTableWidgetItem(var.variableName));
        m_variableTable->setItem(i, 1, new QTableWidgetItem(var.sourceChannel));
        m_variableTable->setItem(i, 2, new QTableWidgetItem(var.sourceCollector));
        m_variableTable->setItem(i, 3, new QTableWidgetItem(DataTypeUtils::dataTypeToString(var.dataType)));
        m_variableTable->setItem(i, 4, new QTableWidgetItem(DataTypeUtils::registerTypeToString(var.registerType)));
        m_variableTable->setItem(i, 5, new QTableWidgetItem(var.comment));
        m_variableTable->setItem(i, 6, new QTableWidgetItem(var.fullId));
    }
    
    m_countLabel->setText(tr("共 %1 个变量").arg(variables.size()));
}

void SystemVariableManagerDialog::updateFilterOptions() {
    m_channelFilter->blockSignals(true);
    m_channelFilter->clear();
    m_channelFilter->addItem(tr("全部通道"), "");
    
    QStringList channels = SystemVariableManager::instance().getAllChannelNames();
    for (const QString& channel : channels) {
        m_channelFilter->addItem(channel, channel);
    }
    
    m_channelFilter->blockSignals(false);
    
    m_collectorFilter->blockSignals(true);
    m_collectorFilter->clear();
    m_collectorFilter->addItem(tr("全部"), "");
    m_collectorFilter->blockSignals(false);
}

} // namespace ModbusPlexLink
