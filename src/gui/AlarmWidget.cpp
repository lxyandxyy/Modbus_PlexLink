#include "AlarmWidget.h"

#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>

#include "AlarmRuleDialog.h"
#include "core/ChannelManager.h"

namespace ModbusPlexLink {

AlarmWidget::AlarmWidget(AlarmManager* alarmManager,
                         ChannelManager* channelManager, QWidget* parent)
    : QWidget(parent),
      m_alarmManager(alarmManager),
      m_channelManager(channelManager),
      m_tabWidget(nullptr),
      m_activeAlarmsTable(nullptr),
      m_acknowledgeBtn(nullptr),
      m_clearBtn(nullptr),
      m_acknowledgeAllBtn(nullptr),
      m_clearAllBtn(nullptr),
      m_statsLabel(nullptr),
      m_historyTable(nullptr),
      m_refreshHistoryBtn(nullptr),
      m_historyDaysCombo(nullptr),
      m_rulesTable(nullptr),
      m_addRuleBtn(nullptr),
      m_editRuleBtn(nullptr),
      m_deleteRuleBtn(nullptr),
      m_toggleRuleBtn(nullptr),
      m_priorityFilter(nullptr),
      m_recordingsTable(nullptr),
      m_viewRecordingBtn(nullptr),
      m_exportRecordingBtn(nullptr),
      m_playbackRecordingBtn(nullptr),
      m_deleteRecordingBtn(nullptr),
      m_recordingInfoLabel(nullptr) {
  setupUi();

  // 连接报警管理器信号（使用 Qt::QueuedConnection 避免死锁）
  connect(m_alarmManager, &AlarmManager::alarmTriggered, this,
          &AlarmWidget::onAlarmTriggered, Qt::QueuedConnection);
  connect(m_alarmManager, &AlarmManager::alarmAcknowledged, this,
          &AlarmWidget::onAlarmAcknowledged, Qt::QueuedConnection);
  connect(m_alarmManager, &AlarmManager::alarmCleared, this,
          &AlarmWidget::onAlarmCleared, Qt::QueuedConnection);

  // 连接录波信号
  connect(m_alarmManager, &AlarmManager::recordingStarted, this,
          &AlarmWidget::onRecordingStarted, Qt::QueuedConnection);
  connect(m_alarmManager, &AlarmManager::recordingCompleted, this,
          &AlarmWidget::onRecordingCompleted, Qt::QueuedConnection);

  refreshDisplay();
}

AlarmWidget::~AlarmWidget() {}

void AlarmWidget::setupUi() {
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);

  // 标题
  QLabel* titleLabel = new QLabel(tr("报警管理"), this);
  QFont titleFont = titleLabel->font();
  titleFont.setPointSize(14);
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);
  mainLayout->addWidget(titleLabel);

  // 标签页
  m_tabWidget = new QTabWidget(this);

  // === 标签页1: 活动报警 ===
  QWidget* activeTab = new QWidget();
  QVBoxLayout* activeLayout = new QVBoxLayout(activeTab);

  // 统计信息
  m_statsLabel = new QLabel(this);
  m_statsLabel->setStyleSheet(
      "QLabel { padding: 5px; background: #f0f0f0; border: 1px solid #ccc; }");
  activeLayout->addWidget(m_statsLabel);

  // 操作按钮
  QHBoxLayout* activeToolLayout = new QHBoxLayout();
  m_acknowledgeBtn = new QPushButton(tr("确认选中"), this);
  connect(m_acknowledgeBtn, &QPushButton::clicked, this,
          &AlarmWidget::onAcknowledgeAlarm);
  activeToolLayout->addWidget(m_acknowledgeBtn);

  m_clearBtn = new QPushButton(tr("清除选中"), this);
  connect(m_clearBtn, &QPushButton::clicked, this, &AlarmWidget::onClearAlarm);
  activeToolLayout->addWidget(m_clearBtn);

  activeToolLayout->addSpacing(20);

  m_acknowledgeAllBtn = new QPushButton(tr("确认全部"), this);
  connect(m_acknowledgeAllBtn, &QPushButton::clicked, this,
          &AlarmWidget::onAcknowledgeAll);
  activeToolLayout->addWidget(m_acknowledgeAllBtn);

  m_clearAllBtn = new QPushButton(tr("清除全部"), this);
  connect(m_clearAllBtn, &QPushButton::clicked, this, &AlarmWidget::onClearAll);
  activeToolLayout->addWidget(m_clearAllBtn);

  activeToolLayout->addStretch();
  activeLayout->addLayout(activeToolLayout);

  // 活动报警表格
  m_activeAlarmsTable = new QTableWidget(this);
  m_activeAlarmsTable->setColumnCount(8);
  m_activeAlarmsTable->setHorizontalHeaderLabels(
      {tr("优先级"), tr("类型"), tr("状态"), tr("通道"), tr("标签"), tr("值"),
       tr("消息"), tr("触发时间")});
  m_activeAlarmsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_activeAlarmsTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_activeAlarmsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_activeAlarmsTable->setAlternatingRowColors(true);
  m_activeAlarmsTable->horizontalHeader()->setStretchLastSection(true);
  m_activeAlarmsTable->setColumnWidth(0, 80);
  m_activeAlarmsTable->setColumnWidth(1, 100);
  m_activeAlarmsTable->setColumnWidth(2, 80);
  m_activeAlarmsTable->setColumnWidth(3, 100);
  m_activeAlarmsTable->setColumnWidth(4, 150);
  m_activeAlarmsTable->setColumnWidth(5, 100);
  activeLayout->addWidget(m_activeAlarmsTable);

  m_tabWidget->addTab(activeTab, tr("活动报警"));

  // === 标签页2: 报警历史 ===
  QWidget* historyTab = new QWidget();
  QVBoxLayout* historyLayout = new QVBoxLayout(historyTab);

  // 工具栏
  QHBoxLayout* historyToolLayout = new QHBoxLayout();
  historyToolLayout->addWidget(new QLabel(tr("查询时间范围:"), this));
  m_historyDaysCombo = new QComboBox(this);
  m_historyDaysCombo->addItems(
      {tr("最近1天"), tr("最近3天"), tr("最近7天"), tr("最近30天")});
  m_historyDaysCombo->setCurrentIndex(1);
  historyToolLayout->addWidget(m_historyDaysCombo);

  m_refreshHistoryBtn = new QPushButton(tr("刷新"), this);
  connect(m_refreshHistoryBtn, &QPushButton::clicked, this,
          &AlarmWidget::onRefreshHistory);
  historyToolLayout->addWidget(m_refreshHistoryBtn);

  historyToolLayout->addStretch();
  historyLayout->addLayout(historyToolLayout);

  // 历史表格
  m_historyTable = new QTableWidget(this);
  m_historyTable->setColumnCount(10);
  m_historyTable->setHorizontalHeaderLabels(
      {tr("优先级"), tr("类型"), tr("通道"), tr("标签"), tr("值"), tr("消息"),
       tr("触发时间"), tr("确认时间"), tr("清除时间"), tr("确认人")});
  m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_historyTable->setAlternatingRowColors(true);
  m_historyTable->horizontalHeader()->setStretchLastSection(true);
  historyLayout->addWidget(m_historyTable);

  m_tabWidget->addTab(historyTab, tr("报警历史"));

  // === 标签页3: 规则管理 ===
  QWidget* rulesTab = new QWidget();
  QVBoxLayout* rulesLayout = new QVBoxLayout(rulesTab);

  // 工具栏
  QHBoxLayout* rulesToolLayout = new QHBoxLayout();
  m_addRuleBtn = new QPushButton(tr("新建规则"), this);
  connect(m_addRuleBtn, &QPushButton::clicked, this, &AlarmWidget::onAddRule);
  rulesToolLayout->addWidget(m_addRuleBtn);

  m_editRuleBtn = new QPushButton(tr("编辑规则"), this);
  connect(m_editRuleBtn, &QPushButton::clicked, this, &AlarmWidget::onEditRule);
  rulesToolLayout->addWidget(m_editRuleBtn);

  m_deleteRuleBtn = new QPushButton(tr("删除规则"), this);
  connect(m_deleteRuleBtn, &QPushButton::clicked, this,
          &AlarmWidget::onDeleteRule);
  rulesToolLayout->addWidget(m_deleteRuleBtn);

  m_toggleRuleBtn = new QPushButton(tr("启用/禁用"), this);
  connect(m_toggleRuleBtn, &QPushButton::clicked, this,
          &AlarmWidget::onToggleRule);
  rulesToolLayout->addWidget(m_toggleRuleBtn);

  rulesToolLayout->addSpacing(20);
  rulesToolLayout->addWidget(new QLabel(tr("优先级过滤:"), this));
  m_priorityFilter = new QComboBox(this);
  m_priorityFilter->addItems(
      {tr("全部"), tr("低"), tr("中"), tr("高"), tr("紧急")});
  connect(m_priorityFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &AlarmWidget::onPriorityFilterChanged);
  rulesToolLayout->addWidget(m_priorityFilter);

  rulesToolLayout->addStretch();
  rulesLayout->addLayout(rulesToolLayout);

  // 规则表格
  m_rulesTable = new QTableWidget(this);
  m_rulesTable->setColumnCount(8);
  m_rulesTable->setHorizontalHeaderLabels(
      {tr("启用"), tr("规则名称"), tr("优先级"), tr("类型"), tr("通道"),
       tr("标签"), tr("条件"), tr("延时(秒)")});
  m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_rulesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_rulesTable->setAlternatingRowColors(true);
  m_rulesTable->horizontalHeader()->setStretchLastSection(true);
  rulesLayout->addWidget(m_rulesTable);

  m_tabWidget->addTab(rulesTab, tr("规则管理"));

  // === 标签页4: 录波数据 ===
  setupRecordingTab();

  mainLayout->addWidget(m_tabWidget);
}

void AlarmWidget::refreshDisplay() {
  updateActiveAlarmsTable();
  updateHistoryTable();
  updateRulesTable();
  updateStatistics();
  updateRecordingsTable();
}

void AlarmWidget::onAlarmTriggered(const AlarmEvent& event) {
  Q_UNUSED(event);
  updateActiveAlarmsTable();
  updateStatistics();

  // 可选：切换到活动报警标签页
  m_tabWidget->setCurrentIndex(0);
}

void AlarmWidget::onAlarmAcknowledged(const QString& eventId) {
  Q_UNUSED(eventId);
  updateActiveAlarmsTable();
  updateStatistics();
}

void AlarmWidget::onAlarmCleared(const QString& eventId) {
  Q_UNUSED(eventId);
  updateActiveAlarmsTable();
  updateHistoryTable();
  updateStatistics();
}

void AlarmWidget::onAcknowledgeAlarm() {
  int row = m_activeAlarmsTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, tr("提示"), tr("请选择要确认的报警"));
    return;
  }

  QString eventId =
      m_activeAlarmsTable->item(row, 0)->data(Qt::UserRole).toString();

  QString user =
      QInputDialog::getText(this, tr("确认报警"), tr("确认人（可选）:"),
                            QLineEdit::Normal, tr("操作员"));

  if (m_alarmManager->acknowledgeAlarm(eventId, user)) {
    QMessageBox::information(this, tr("成功"), tr("报警已确认"));
  }
}

void AlarmWidget::onClearAlarm() {
  int row = m_activeAlarmsTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, tr("提示"), tr("请选择要清除的报警"));
    return;
  }

  QString eventId =
      m_activeAlarmsTable->item(row, 0)->data(Qt::UserRole).toString();

  auto reply =
      QMessageBox::question(this, tr("清除报警"), tr("确定要清除该报警吗？"),
                            QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    if (m_alarmManager->clearAlarm(eventId)) {
      QMessageBox::information(this, tr("成功"), tr("报警已清除"));
    }
  }
}

void AlarmWidget::onAcknowledgeAll() {
  auto reply = QMessageBox::question(this, tr("确认全部报警"),
                                     tr("确定要确认所有活动报警吗？"),
                                     QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    QString user =
        QInputDialog::getText(this, tr("确认报警"), tr("确认人（可选）:"),
                              QLineEdit::Normal, tr("操作员"));

    QList<AlarmEvent> activeAlarms = m_alarmManager->getActiveAlarms();
    int count = 0;
    for (const AlarmEvent& event : activeAlarms) {
      if (m_alarmManager->acknowledgeAlarm(event.id, user)) {
        count++;
      }
    }

    QMessageBox::information(this, tr("成功"),
                             tr("已确认 %1 条报警").arg(count));
  }
}

void AlarmWidget::onClearAll() {
  auto reply =
      QMessageBox::question(this, tr("清除全部报警"),
                            tr("确定要清除所有活动报警吗？此操作不可撤销！"),
                            QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    QList<AlarmEvent> activeAlarms = m_alarmManager->getActiveAlarms();
    int count = 0;
    for (const AlarmEvent& event : activeAlarms) {
      if (m_alarmManager->clearAlarm(event.id)) {
        count++;
      }
    }

    QMessageBox::information(this, tr("成功"),
                             tr("已清除 %1 条报警").arg(count));
  }
}

void AlarmWidget::onAddRule() {
  if (!m_channelManager) {
    QMessageBox::warning(this, tr("错误"), tr("通道管理器未初始化"));
    return;
  }

  AlarmRuleDialog dialog(m_channelManager, nullptr, this);
  if (dialog.exec() == QDialog::Accepted) {
    AlarmRule rule = dialog.getRule();
    QString ruleId = m_alarmManager->addRule(rule);
    if (!ruleId.isEmpty()) {
      QMessageBox::information(this, tr("成功"), tr("规则已添加"));
      updateRulesTable();
      // 自动保存配置
      m_alarmManager->saveConfig("alarm_config.json");
    } else {
      QMessageBox::warning(this, tr("错误"), tr("添加规则失败"));
    }
  }
}

void AlarmWidget::onEditRule() {
  if (!m_channelManager) {
    QMessageBox::warning(this, tr("错误"), tr("通道管理器未初始化"));
    return;
  }

  int row = m_rulesTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, tr("提示"), tr("请选择要编辑的规则"));
    return;
  }

  QString ruleId = m_rulesTable->item(row, 0)->data(Qt::UserRole).toString();
  AlarmRule rule = m_alarmManager->getRule(ruleId);

  AlarmRuleDialog dialog(m_channelManager, &rule, this);
  if (dialog.exec() == QDialog::Accepted) {
    AlarmRule updatedRule = dialog.getRule();
    if (m_alarmManager->updateRule(ruleId, updatedRule)) {
      QMessageBox::information(this, tr("成功"), tr("规则已更新"));
      updateRulesTable();
      // 自动保存配置
      m_alarmManager->saveConfig("alarm_config.json");
    } else {
      QMessageBox::warning(this, tr("错误"), tr("更新规则失败"));
    }
  }
}

void AlarmWidget::onDeleteRule() {
  int row = m_rulesTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, tr("提示"), tr("请选择要删除的规则"));
    return;
  }

  QString ruleId = m_rulesTable->item(row, 0)->data(Qt::UserRole).toString();
  AlarmRule rule = m_alarmManager->getRule(ruleId);

  auto reply = QMessageBox::question(
      this, tr("删除规则"), tr("确定要删除规则 \"%1\" 吗？").arg(rule.name),
      QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    if (m_alarmManager->removeRule(ruleId)) {
      QMessageBox::information(this, tr("成功"), tr("规则已删除"));
      updateRulesTable();
      // 自动保存配置
      m_alarmManager->saveConfig("alarm_config.json");
    }
  }
}

void AlarmWidget::onToggleRule() {
  int row = m_rulesTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, tr("提示"), tr("请选择要切换的规则"));
    return;
  }

  QString ruleId = m_rulesTable->item(row, 0)->data(Qt::UserRole).toString();
  AlarmRule rule = m_alarmManager->getRule(ruleId);

  bool newState = !rule.enabled;
  if (m_alarmManager->enableRule(ruleId, newState)) {
    QMessageBox::information(
        this, tr("成功"),
        tr("规则已%1").arg(newState ? tr("启用") : tr("禁用")));
    updateRulesTable();
    // 自动保存配置
    m_alarmManager->saveConfig("alarm_config.json");
  }
}

void AlarmWidget::onPriorityFilterChanged(int index) {
  Q_UNUSED(index);
  updateRulesTable();
}

void AlarmWidget::onRefreshHistory() { updateHistoryTable(); }

void AlarmWidget::updateActiveAlarmsTable() {
  m_activeAlarmsTable->setSortingEnabled(false);
  m_activeAlarmsTable->setRowCount(0);

  QList<AlarmEvent> activeAlarms = m_alarmManager->getActiveAlarms();

  for (const AlarmEvent& event : activeAlarms) {
    int row = m_activeAlarmsTable->rowCount();
    m_activeAlarmsTable->insertRow(row);

    // 存储事件ID
    QTableWidgetItem* priorityItem =
        new QTableWidgetItem(alarmPriorityToString(event.priority));
    priorityItem->setData(Qt::UserRole, event.id);
    priorityItem->setBackground(alarmPriorityColor(event.priority));
    m_activeAlarmsTable->setItem(row, 0, priorityItem);

    m_activeAlarmsTable->setItem(
        row, 1, new QTableWidgetItem(alarmTypeToString(event.type)));
    m_activeAlarmsTable->setItem(
        row, 2, new QTableWidgetItem(alarmStateToString(event.state)));
    m_activeAlarmsTable->setItem(row, 3,
                                 new QTableWidgetItem(event.channelName));
    m_activeAlarmsTable->setItem(row, 4, new QTableWidgetItem(event.tagName));
    m_activeAlarmsTable->setItem(row, 5,
                                 new QTableWidgetItem(event.value.toString()));
    m_activeAlarmsTable->setItem(row, 6, new QTableWidgetItem(event.message));
    m_activeAlarmsTable->setItem(
        row, 7,
        new QTableWidgetItem(event.activeTime.toString("yyyy-MM-dd HH:mm:ss")));
  }

  m_activeAlarmsTable->setSortingEnabled(true);
}

void AlarmWidget::updateHistoryTable() {
  m_historyTable->setRowCount(0);

  // 获取时间范围
  int days = 3;  // 默认3天
  switch (m_historyDaysCombo->currentIndex()) {
    case 0:
      days = 1;
      break;
    case 1:
      days = 3;
      break;
    case 2:
      days = 7;
      break;
    case 3:
      days = 30;
      break;
  }

  QDateTime endTime = QDateTime::currentDateTime();
  QDateTime startTime = endTime.addDays(-days);

  QList<AlarmEvent> history =
      m_alarmManager->getAlarmHistory(startTime, endTime);

  for (const AlarmEvent& event : history) {
    int row = m_historyTable->rowCount();
    m_historyTable->insertRow(row);

    QTableWidgetItem* priorityItem =
        new QTableWidgetItem(alarmPriorityToString(event.priority));
    priorityItem->setBackground(alarmPriorityColor(event.priority));
    m_historyTable->setItem(row, 0, priorityItem);

    m_historyTable->setItem(
        row, 1, new QTableWidgetItem(alarmTypeToString(event.type)));
    m_historyTable->setItem(row, 2, new QTableWidgetItem(event.channelName));
    m_historyTable->setItem(row, 3, new QTableWidgetItem(event.tagName));
    m_historyTable->setItem(row, 4,
                            new QTableWidgetItem(event.value.toString()));
    m_historyTable->setItem(row, 5, new QTableWidgetItem(event.message));
    m_historyTable->setItem(
        row, 6,
        new QTableWidgetItem(event.activeTime.toString("yyyy-MM-dd HH:mm:ss")));
    m_historyTable->setItem(
        row, 7,
        new QTableWidgetItem(
            event.acknowledgedTime.isValid()
                ? event.acknowledgedTime.toString("yyyy-MM-dd HH:mm:ss")
                : "-"));
    m_historyTable->setItem(
        row, 8,
        new QTableWidgetItem(
            event.clearedTime.isValid()
                ? event.clearedTime.toString("yyyy-MM-dd HH:mm:ss")
                : "-"));
    m_historyTable->setItem(row, 9, new QTableWidgetItem(event.acknowledgedBy));
  }
}

void AlarmWidget::updateRulesTable() {
  m_rulesTable->setRowCount(0);

  QList<AlarmRule> rules = m_alarmManager->getAllRules();

  // 优先级过滤
  AlarmPriority filterPriority =
      static_cast<AlarmPriority>(m_priorityFilter->currentIndex() - 1);

  for (const AlarmRule& rule : rules) {
    // 应用过滤
    if (m_priorityFilter->currentIndex() > 0 &&
        rule.priority != filterPriority) {
      continue;
    }

    int row = m_rulesTable->rowCount();
    m_rulesTable->insertRow(row);

    // 存储规则ID
    QTableWidgetItem* enabledItem =
        new QTableWidgetItem(rule.enabled ? tr("是") : tr("否"));
    enabledItem->setData(Qt::UserRole, rule.id);
    enabledItem->setBackground(rule.enabled ? QColor(200, 255, 200)
                                            : QColor(220, 220, 220));
    m_rulesTable->setItem(row, 0, enabledItem);

    m_rulesTable->setItem(row, 1, new QTableWidgetItem(rule.name));
    m_rulesTable->setItem(
        row, 2, new QTableWidgetItem(alarmPriorityToString(rule.priority)));
    m_rulesTable->setItem(row, 3,
                          new QTableWidgetItem(alarmTypeToString(rule.type)));
    m_rulesTable->setItem(row, 4, new QTableWidgetItem(rule.channelName));
    m_rulesTable->setItem(row, 5, new QTableWidgetItem(rule.tagName));

    // 条件
    QString condition;
    switch (rule.type) {
      case AlarmType::HighLimit:
        condition = QString("> %1").arg(rule.highLimit);
        break;
      case AlarmType::LowLimit:
        condition = QString("< %1").arg(rule.lowLimit);
        break;
      case AlarmType::HighHighLimit:
        condition = QString("> %1").arg(rule.highHighLimit);
        break;
      case AlarmType::LowLowLimit:
        condition = QString("< %1").arg(rule.lowLowLimit);
        break;
      default:
        condition = "-";
        break;
    }
    m_rulesTable->setItem(row, 6, new QTableWidgetItem(condition));
    m_rulesTable->setItem(
        row, 7, new QTableWidgetItem(QString::number(rule.delaySeconds)));
  }
}

void AlarmWidget::updateStatistics() {
  int total = m_alarmManager->getActiveAlarmCount();
  int critical = m_alarmManager->getAlarmCount(AlarmPriority::Critical);
  int high = m_alarmManager->getAlarmCount(AlarmPriority::High);
  int medium = m_alarmManager->getAlarmCount(AlarmPriority::Medium);
  int low = m_alarmManager->getAlarmCount(AlarmPriority::Low);

  QString stats = tr("活动报警: %1 条 | 紧急: %2 | 高: %3 | 中: %4 | 低: %5")
                      .arg(total)
                      .arg(critical)
                      .arg(high)
                      .arg(medium)
                      .arg(low);

  m_statsLabel->setText(stats);

  // 根据严重程度设置颜色
  if (critical > 0) {
    m_statsLabel->setStyleSheet(
        "QLabel { padding: 5px; background: #ffcccc; border: 2px solid red; "
        "font-weight: bold; }");
  } else if (high > 0) {
    m_statsLabel->setStyleSheet(
        "QLabel { padding: 5px; background: #ffe6cc; border: 2px solid orange; "
        "font-weight: bold; }");
  } else if (total > 0) {
    m_statsLabel->setStyleSheet(
        "QLabel { padding: 5px; background: #ffffcc; border: 1px solid #ccc; "
        "}");
  } else {
    m_statsLabel->setStyleSheet(
        "QLabel { padding: 5px; background: #ccffcc; border: 1px solid #ccc; "
        "}");
  }
}

QString AlarmWidget::alarmTypeToString(AlarmType type) const {
  switch (type) {
    case AlarmType::HighLimit:
      return tr("高限");
    case AlarmType::LowLimit:
      return tr("低限");
    case AlarmType::HighHighLimit:
      return tr("高高限");
    case AlarmType::LowLowLimit:
      return tr("低低限");
    case AlarmType::ValueChange:
      return tr("值变化");
    case AlarmType::DataQuality:
      return tr("数据质量");
    case AlarmType::ConnectionLost:
      return tr("连接丢失");
    case AlarmType::Custom:
      return tr("自定义");
    default:
      return tr("未知");
  }
}

QString AlarmWidget::alarmPriorityToString(AlarmPriority priority) const {
  switch (priority) {
    case AlarmPriority::Low:
      return tr("低");
    case AlarmPriority::Medium:
      return tr("中");
    case AlarmPriority::High:
      return tr("高");
    case AlarmPriority::Critical:
      return tr("紧急");
    default:
      return tr("未知");
  }
}

QString AlarmWidget::alarmStateToString(AlarmState state) const {
  switch (state) {
    case AlarmState::Active:
      return tr("活动");
    case AlarmState::Acknowledged:
      return tr("已确认");
    case AlarmState::Cleared:
      return tr("已清除");
    default:
      return tr("未知");
  }
}

QColor AlarmWidget::alarmPriorityColor(AlarmPriority priority) const {
  switch (priority) {
    case AlarmPriority::Critical:
      return QColor(255, 200, 200);  // 红
    case AlarmPriority::High:
      return QColor(255, 230, 200);  // 橙
    case AlarmPriority::Medium:
      return QColor(255, 255, 200);  // 黄
    case AlarmPriority::Low:
      return QColor(220, 220, 255);  // 蓝
    default:
      return Qt::white;
  }
}

// ==================== 录波相关函数实现 ====================

void AlarmWidget::setupRecordingTab() {
  QWidget* recordingTab = new QWidget();
  QVBoxLayout* recordingLayout = new QVBoxLayout(recordingTab);
  recordingLayout->setContentsMargins(10, 10, 10, 10);
  recordingLayout->setSpacing(10);

  // 说明信息
  m_recordingInfoLabel = new QLabel(tr("💡 "
                                       "此处显示告警触发时自动录波的数据。点击‘"
                                       "回放’将跳转到系统录波界面查看波形。"),
                                    this);
  m_recordingInfoLabel->setWordWrap(true);
  m_recordingInfoLabel->setStyleSheet(
      "QLabel { padding: 8px; background: #EEF2FF; border: 1px solid #C7D2FE; "
      "border-radius: 4px; color: #4338CA; }");
  recordingLayout->addWidget(m_recordingInfoLabel);

  // 工具栏
  QHBoxLayout* toolLayout = new QHBoxLayout();
  toolLayout->setSpacing(8);

  m_viewRecordingBtn = new QPushButton(tr("📋 查看详情"), this);
  m_viewRecordingBtn->setStyleSheet("QPushButton { padding: 6px 12px; }");
  connect(m_viewRecordingBtn, &QPushButton::clicked, this,
          &AlarmWidget::onViewRecording);
  toolLayout->addWidget(m_viewRecordingBtn);

  m_exportRecordingBtn = new QPushButton(tr("📥 导出CSV"), this);
  m_exportRecordingBtn->setStyleSheet("QPushButton { padding: 6px 12px; }");
  connect(m_exportRecordingBtn, &QPushButton::clicked, this,
          &AlarmWidget::onExportRecording);
  toolLayout->addWidget(m_exportRecordingBtn);

  m_playbackRecordingBtn = new QPushButton(tr("📊 在录波界面回放"), this);
  m_playbackRecordingBtn->setStyleSheet(
      "QPushButton { padding: 6px 12px; background: #3B82F6; color: white; "
      "border: none; border-radius: 4px; font-weight: bold; }"
      "QPushButton:hover { background: #2563EB; }");
  connect(m_playbackRecordingBtn, &QPushButton::clicked, this,
          &AlarmWidget::onPlaybackRecording);
  toolLayout->addWidget(m_playbackRecordingBtn);

  toolLayout->addSpacing(20);

  m_deleteRecordingBtn = new QPushButton(tr("🗑️ 删除"), this);
  m_deleteRecordingBtn->setStyleSheet(
      "QPushButton { padding: 6px 12px; color: #DC2626; }");
  connect(m_deleteRecordingBtn, &QPushButton::clicked, this,
          &AlarmWidget::onDeleteRecording);
  toolLayout->addWidget(m_deleteRecordingBtn);

  toolLayout->addStretch();
  recordingLayout->addLayout(toolLayout);

  // 录波数据表格
  m_recordingsTable = new QTableWidget(this);
  m_recordingsTable->setColumnCount(7);
  m_recordingsTable->setHorizontalHeaderLabels(
      {tr("报警事件"), tr("规则名称"), tr("开始时间"), tr("结束时间"),
       tr("数据点"), tr("标签"), tr("CSV文件")});
  m_recordingsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_recordingsTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_recordingsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_recordingsTable->setAlternatingRowColors(true);
  m_recordingsTable->horizontalHeader()->setStretchLastSection(true);
  m_recordingsTable->setStyleSheet(
      "QTableWidget { border: 1px solid #E2E8F0; border-radius: 4px; }"
      "QHeaderView::section { background: #F8FAFC; padding: 8px; "
      "border: none; border-bottom: 1px solid #E2E8F0; font-weight: bold; }");

  // 设置列宽
  m_recordingsTable->setColumnWidth(0, 100);
  m_recordingsTable->setColumnWidth(1, 150);
  m_recordingsTable->setColumnWidth(2, 150);
  m_recordingsTable->setColumnWidth(3, 150);
  m_recordingsTable->setColumnWidth(4, 80);
  m_recordingsTable->setColumnWidth(5, 80);

  // 双击回放
  connect(m_recordingsTable, &QTableWidget::doubleClicked, this,
          &AlarmWidget::onPlaybackRecording);

  recordingLayout->addWidget(m_recordingsTable);

  m_tabWidget->addTab(recordingTab, tr("📹 录波数据"));
}

void AlarmWidget::updateRecordingsTable() {
  m_recordingsTable->setRowCount(0);

  QList<AlarmEvent> alarmsWithRecording =
      m_alarmManager->getAlarmsWithRecording();

  for (const AlarmEvent& event : alarmsWithRecording) {
    int row = m_recordingsTable->rowCount();
    m_recordingsTable->insertRow(row);

    AlarmRecordingData data = m_alarmManager->getRecordingData(event.id);

    QTableWidgetItem* idItem = new QTableWidgetItem(event.id.left(8) + "...");
    idItem->setData(Qt::UserRole, event.id);
    idItem->setToolTip(event.id);
    m_recordingsTable->setItem(row, 0, idItem);

    m_recordingsTable->setItem(row, 1, new QTableWidgetItem(event.ruleName));
    m_recordingsTable->setItem(
        row, 2,
        new QTableWidgetItem(data.startTime.toString("yyyy-MM-dd HH:mm:ss")));
    m_recordingsTable->setItem(
        row, 3,
        new QTableWidgetItem(data.endTime.isValid()
                                 ? data.endTime.toString("yyyy-MM-dd HH:mm:ss")
                                 : tr("录波中...")));
    m_recordingsTable->setItem(
        row, 4, new QTableWidgetItem(QString::number(data.totalDataPoints)));
    m_recordingsTable->setItem(
        row, 5,
        new QTableWidgetItem(QString::number(data.recordedTags.size())));
    m_recordingsTable->setItem(
        row, 6,
        new QTableWidgetItem(data.csvFilePath.isEmpty()
                                 ? "-"
                                 : QFileInfo(data.csvFilePath).fileName()));
  }
}

void AlarmWidget::onRecordingStarted(const QString& alarmEventId) {
  Q_UNUSED(alarmEventId);
  updateRecordingsTable();

  // 切换到录波标签页
  if (m_tabWidget->count() >= 4) {
    // 可选：自动切换
    // m_tabWidget->setCurrentIndex(3);
  }
}

void AlarmWidget::onRecordingCompleted(const QString& alarmEventId,
                                       const QString& csvFilePath) {
  Q_UNUSED(alarmEventId);
  Q_UNUSED(csvFilePath);
  updateRecordingsTable();
}

void AlarmWidget::onViewRecording() {
  int row = m_recordingsTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, tr("提示"), tr("请选择要查看的录波数据"));
    return;
  }

  QString alarmEventId =
      m_recordingsTable->item(row, 0)->data(Qt::UserRole).toString();
  AlarmRecordingData data = m_alarmManager->getRecordingData(alarmEventId);

  QString info =
      tr("报警事件ID: %1\n"
         "开始时间: %2\n"
         "结束时间: %3\n"
         "数据点数: %4\n"
         "录波标签: %5\n"
         "CSV文件: %6")
          .arg(alarmEventId)
          .arg(data.startTime.toString("yyyy-MM-dd HH:mm:ss"))
          .arg(data.endTime.isValid()
                   ? data.endTime.toString("yyyy-MM-dd HH:mm:ss")
                   : tr("未完成"))
          .arg(data.totalDataPoints)
          .arg(data.recordedTags.join(", "))
          .arg(data.csvFilePath.isEmpty() ? tr("无") : data.csvFilePath);

  QMessageBox::information(this, tr("录波数据详情"), info);
}

void AlarmWidget::onExportRecording() {
  int row = m_recordingsTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, tr("提示"), tr("请选择要导出的录波数据"));
    return;
  }

  QString alarmEventId =
      m_recordingsTable->item(row, 0)->data(Qt::UserRole).toString();

  QString filename = QFileDialog::getSaveFileName(
      this, tr("导出CSV"),
      QString("alarm_recording_%1.csv")
          .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
      tr("CSV文件 (*.csv)"));

  if (filename.isEmpty()) return;

  if (m_alarmManager->exportRecordingToCsv(alarmEventId, filename)) {
    QMessageBox::information(this, tr("成功"),
                             tr("录波数据已导出到:\n%1").arg(filename));
    updateRecordingsTable();
  } else {
    QMessageBox::warning(this, tr("错误"), tr("导出失败"));
  }
}

void AlarmWidget::onPlaybackRecording() {
  int row = m_recordingsTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, tr("提示"), tr("请选择要回放的录波数据"));
    return;
  }

  QString alarmEventId =
      m_recordingsTable->item(row, 0)->data(Qt::UserRole).toString();
  AlarmRecordingData data = m_alarmManager->getRecordingData(alarmEventId);

  if (data.data.isEmpty()) {
    QMessageBox::warning(this, tr("错误"), tr("录波数据为空或不可用"));
    return;
  }

  // 发射信号，请求在系统录波界面中回放
  emit requestPlaybackInRecorder(data.csvFilePath, data);
}

void AlarmWidget::onDeleteRecording() {
  int row = m_recordingsTable->currentRow();
  if (row < 0) {
    QMessageBox::information(this, tr("提示"), tr("请选择要删除的录波数据"));
    return;
  }

  auto reply =
      QMessageBox::question(this, tr("删除录波数据"),
                            tr("确定要删除选中的录波数据吗？此操作不可撤销。"),
                            QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    // 目前AlarmManager没有删除录波数据的接口，这里只提示
    QMessageBox::information(this, tr("提示"),
                             tr("录波数据将在程序重启或自动清理时移除。"));
  }
}

}  // namespace ModbusPlexLink
