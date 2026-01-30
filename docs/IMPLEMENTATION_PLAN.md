# Modbus PlexLink 功能改进实施方案

> **文档版本**: 1.1.0  
> **创建日期**: 2026-01-29  
> **最后更新**: 2026-01-30

---

## 📋 需求总览

| ID | 需求名称 | 优先级 | 状态 | 预计工时 |
|----|----------|--------|------|----------|
| R1 | 历史告警持久化（SQLite） | P0 | ✅ 已完成 | 1天 |
| R2 | 历史告警不显示修复 | P0 | ✅ 已完成 | 0.5天 |
| R3 | 历史告警查看录波和回放 | P0 | ✅ 已完成 | 1天 |
| R4 | 告警/录波界面显示当前模式 | P1 | ✅ 已完成 | 0.5天 |
| R5 | 远程模式告警管理 | P1 | ✅ 已完成 | 2天 |
| R6 | 远程模式录波（本地录波远程数据） | P1 | ✅ 已完成 | 1.5天 |
| R7 | 通道类型区分（采集/服务） | P2 | ✅ 已完成 | 2天 |
| R8 | 全局数据模型（GlobalDataModel） | P2 | ✅ 已完成 | 2天 |
| R9 | 系统变量管理器 | P2 | ✅ 已完成 | 2天 |
| R10 | 批量创建通道功能 | P2 | ✅ 已完成 | 2天 |

**状态说明**: ⬜ 待开始 | 🔵 进行中 | ✅ 已完成 | ❌ 已取消

---

## 🔴 R1: 历史告警持久化（SQLite）

### 需求描述
将历史告警数据持久化存储到SQLite数据库，解决重启后历史告警丢失的问题。

### 技术方案

#### 1.1 数据库设计

```sql
-- 告警事件表
CREATE TABLE IF NOT EXISTS alarm_events (
    id TEXT PRIMARY KEY,              -- 事件ID (UUID)
    rule_id TEXT NOT NULL,            -- 规则ID
    rule_name TEXT,                   -- 规则名称
    alarm_type INTEGER,               -- 报警类型
    priority INTEGER,                 -- 优先级
    state INTEGER,                    -- 状态
    channel_name TEXT,                -- 通道名称
    tag_name TEXT,                    -- 标签名称
    trigger_value TEXT,               -- 触发值
    message TEXT,                     -- 报警消息
    active_time DATETIME,             -- 激活时间
    acknowledged_time DATETIME,       -- 确认时间
    cleared_time DATETIME,            -- 清除时间
    acknowledged_by TEXT,             -- 确认人
    has_recording INTEGER DEFAULT 0,  -- 是否有录波
    recording_id TEXT,                -- 录波数据ID
    recording_file_path TEXT,         -- 录波文件路径
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 创建索引
CREATE INDEX IF NOT EXISTS idx_alarm_events_active_time ON alarm_events(active_time);
CREATE INDEX IF NOT EXISTS idx_alarm_events_channel ON alarm_events(channel_name);
CREATE INDEX IF NOT EXISTS idx_alarm_events_state ON alarm_events(state);

-- 告警规则表（可选，用于规则持久化）
CREATE TABLE IF NOT EXISTS alarm_rules (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    enabled INTEGER DEFAULT 1,
    channel_name TEXT,
    tag_name TEXT,
    alarm_type INTEGER,
    priority INTEGER,
    high_limit REAL,
    low_limit REAL,
    high_high_limit REAL,
    low_low_limit REAL,
    deadband REAL,
    delay_seconds INTEGER,
    message TEXT,
    recording_config TEXT,           -- JSON格式的录波配置
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

#### 1.2 新增类：AlarmDatabase

```cpp
// src/utils/AlarmDatabase.h

#ifndef ALARMDATABASE_H
#define ALARMDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QMutex>
#include "AlarmManager.h"

namespace ModbusPlexLink {

class AlarmDatabase : public QObject {
    Q_OBJECT
    
public:
    static AlarmDatabase& instance();
    
    // 初始化数据库
    bool initialize(const QString& dbPath = "alarms.db");
    void close();
    
    // 告警事件操作
    bool insertAlarmEvent(const AlarmEvent& event);
    bool updateAlarmEvent(const AlarmEvent& event);
    QList<AlarmEvent> getAlarmHistory(const QDateTime& start, const QDateTime& end, int limit = 1000);
    QList<AlarmEvent> getAlarmsByChannel(const QString& channelName, int limit = 100);
    AlarmEvent getAlarmEvent(const QString& eventId);
    int getTotalAlarmCount();
    
    // 告警规则操作（可选）
    bool saveAlarmRule(const AlarmRule& rule);
    bool deleteAlarmRule(const QString& ruleId);
    QList<AlarmRule> loadAllRules();
    
    // 清理旧数据
    int cleanupOldEvents(int keepDays = 90);
    
signals:
    void databaseError(const QString& error);
    
private:
    AlarmDatabase();
    ~AlarmDatabase();
    
    bool createTables();
    
    QSqlDatabase m_db;
    mutable QMutex m_mutex;
    bool m_initialized;
};

} // namespace ModbusPlexLink

#endif // ALARMDATABASE_H
```

#### 1.3 修改 AlarmManager

```cpp
// src/utils/AlarmManager.cpp 修改

void AlarmManager::triggerAlarm(const AlarmRule& rule, const QString& channelName,
                                const QString& tagName, const QVariant& value) {
    // ... 创建 AlarmEvent ...
    
    // 添加到内存列表
    m_activeAlarms.append(event);
    m_alarmHistory.append(event);
    
    // 持久化到数据库
    AlarmDatabase::instance().insertAlarmEvent(event);
    
    emit alarmTriggered(event);
}

QList<AlarmEvent> AlarmManager::getAlarmHistory(const QDateTime& start, 
                                                 const QDateTime& end) const {
    // 从数据库查询
    return AlarmDatabase::instance().getAlarmHistory(start, end);
}
```

### 实施步骤

- [ ] 1.1 创建 `AlarmDatabase` 类
- [ ] 1.2 实现数据库初始化和表创建
- [ ] 1.3 实现告警事件CRUD操作
- [ ] 1.4 修改 `AlarmManager` 集成数据库
- [ ] 1.5 添加数据清理定时任务
- [ ] 1.6 测试验证

### 验收标准

- [x] 历史告警数据重启后保留
- [x] 支持按时间范围查询历史告警
- [x] 支持按通道筛选告警
- [x] 自动清理90天前的旧数据

---

## 🔴 R2: 历史告警不显示修复

### 需求描述
修复历史告警列表不显示数据的Bug。

### 技术方案

检查并修复 `AlarmWidget::updateHistoryTable()` 方法，确保正确从数据库/内存获取并显示历史告警。

### 实施步骤

- [ ] 2.1 检查 `AlarmWidget::updateHistoryTable()` 逻辑
- [ ] 2.2 确保历史表格正确绑定数据
- [ ] 2.3 添加刷新按钮功能
- [ ] 2.4 添加时间范围筛选
- [ ] 2.5 测试验证

---

## 🔴 R3: 历史告警查看录波和回放

### 需求描述
在历史告警列表中，选中有录波数据的告警，可以点击查看录波并在录波界面进行回放。

### 技术方案

#### 3.1 历史告警表格增加录波列

```cpp
// AlarmWidget.h 新增
QPushButton* m_viewHistoryRecordingBtn;

// AlarmWidget.cpp
void AlarmWidget::setupHistoryTab() {
    // 表格列：时间、规则、通道、标签、值、消息、状态、录波、操作
    m_historyTable->setColumnCount(9);
    
    // 查看录波按钮
    m_viewHistoryRecordingBtn = new QPushButton(tr("📊 查看录波"));
    m_viewHistoryRecordingBtn->setEnabled(false);
    connect(m_viewHistoryRecordingBtn, &QPushButton::clicked,
            this, &AlarmWidget::onViewHistoryRecording);
}

void AlarmWidget::onViewHistoryRecording() {
    int row = m_historyTable->currentRow();
    if (row < 0) return;
    
    QString eventId = m_historyTable->item(row, 0)->data(Qt::UserRole).toString();
    QString csvFilePath = m_historyTable->item(row, 7)->data(Qt::UserRole).toString();
    
    AlarmRecordingData recordingData = m_alarmManager->getRecordingData(eventId);
    emit requestPlaybackInRecorder(csvFilePath, recordingData);
}
```

### 实施步骤

- [ ] 3.1 历史表格增加"录波"列
- [ ] 3.2 添加"查看录波"按钮
- [ ] 3.3 实现 `onViewHistoryRecording()` 槽函数
- [ ] 3.4 MainWindow 处理回放信号
- [ ] 3.5 WaveformRecorder 加载并回放数据
- [ ] 3.6 测试验证

---

## 🟠 R4: 告警/录波界面显示当前模式

### 需求描述
在告警管理界面和录波界面顶部显示当前系统运行模式（本地/远程）。

### 技术方案

#### 4.1 创建模式状态Banner组件

```cpp
// src/gui/ModeStatusBanner.h

class ModeStatusBanner : public QFrame {
    Q_OBJECT
public:
    explicit ModeStatusBanner(QWidget* parent = nullptr);
    
    void setLocalMode();
    void setLocalWithApiMode(quint16 httpPort, quint16 wsPort);
    void setRemoteMode(const QString& remoteHost, bool connected);
    
private:
    QLabel* m_iconLabel;
    QLabel* m_textLabel;
    QLabel* m_detailLabel;
};
```

#### 4.2 样式设计

```cpp
// 本地模式
background: #4CAF50;  // 绿色
text: "🏠 本地模式"

// 本地+API模式
background: #2196F3;  // 蓝色
text: "🌐 本地模式 + API服务 (HTTP:8080, WS:8081)"

// 远程模式
background: #FF9800;  // 橙色
text: "📡 远程模式 - 已连接到 192.168.1.100"

// 远程断开
background: #f44336;  // 红色
text: "📡 远程模式 - 连接断开"
```

### 实施步骤

- [ ] 4.1 创建 `ModeStatusBanner` 组件
- [ ] 4.2 在 `AlarmWidget` 顶部添加Banner
- [ ] 4.3 在 `WaveformRecorderWidget` 顶部添加Banner
- [ ] 4.4 MainWindow 模式变化时更新Banner
- [ ] 4.5 测试验证

---

## 🟠 R5: 远程模式告警管理

### 需求描述
远程模式下，告警管理界面应该操作远程网关的告警配置和历史。

### 技术方案

#### 5.1 扩展 RemoteApiServer 告警API

```cpp
// 新增API端点
GET    /api/alarm-rules                    // 获取所有告警规则
POST   /api/alarm-rules                    // 创建告警规则
PUT    /api/alarm-rules/{id}               // 更新告警规则
DELETE /api/alarm-rules/{id}               // 删除告警规则
PUT    /api/alarm-rules/{id}/enable        // 启用/禁用规则

GET    /api/alarm-history                  // 获取告警历史
GET    /api/alarm-history/{id}             // 获取单个告警详情
GET    /api/alarm-history/{id}/recording   // 获取告警录波数据
POST   /api/alarm-history/{id}/acknowledge // 确认告警
```

#### 5.2 扩展 RemoteClient

```cpp
// src/remote/RemoteClient.h 新增

// 告警规则管理
void getAlarmRules();
void addAlarmRule(const QJsonObject& rule);
void updateAlarmRule(const QString& ruleId, const QJsonObject& rule);
void deleteAlarmRule(const QString& ruleId);

// 告警历史
void getAlarmHistory(int days = 7);
void getAlarmRecording(const QString& eventId);

// 信号
signals:
    void alarmRulesReceived(const QJsonArray& rules);
    void alarmHistoryReceived(const QJsonArray& history);
    void alarmRecordingReceived(const QString& eventId, const QByteArray& csvData);
```

#### 5.3 改造 AlarmWidget 双模式

```cpp
// src/gui/AlarmWidget.h 新增

class AlarmWidget : public QWidget {
    // 模式设置
    void setRemoteMode(bool isRemote, RemoteClient* client = nullptr);
    
private:
    bool m_isRemoteMode = false;
    RemoteClient* m_remoteClient = nullptr;
    
    // 根据模式刷新数据
    void refreshData();
};
```

### 实施步骤

- [ ] 5.1 扩展 RemoteApiServer 告警API
- [ ] 5.2 扩展 RemoteClient 告警方法
- [ ] 5.3 改造 AlarmWidget 支持双模式
- [ ] 5.4 远程模式下规则管理
- [ ] 5.5 远程模式下历史查询
- [ ] 5.6 测试验证

---

## 🟠 R6: 远程模式录波（本地录波远程数据）

### 需求描述
远程模式下，录波功能在本地进行，但数据来源是远程网关推送的实时数据。

### 技术方案

#### 6.1 设计思路

```
┌─────────────────────────────────────────────────────────────┐
│                     远程网关 (Server)                        │
│  ┌──────────┐                                               │
│  │ 采集器   │ → 实时数据                                     │
│  └──────────┘                                               │
│       │                                                      │
│       ▼                                                      │
│  ┌────────────────────────┐                                 │
│  │ WebSocket 推送         │                                 │
│  │ (数据订阅)             │                                 │
│  └────────────┬───────────┘                                 │
└───────────────┼─────────────────────────────────────────────┘
                │ 实时数据流
                ▼
┌─────────────────────────────────────────────────────────────┐
│                     本地客户端 (Client)                      │
│  ┌────────────────────────┐                                 │
│  │ RemoteClient           │                                 │
│  │ (接收WebSocket数据)    │                                 │
│  └────────────┬───────────┘                                 │
│               │                                              │
│               ▼                                              │
│  ┌────────────────────────┐                                 │
│  │ WaveformRecorderWidget │ ← 本地录波                      │
│  │ - 采样远程数据          │                                 │
│  │ - 本地存储             │                                 │
│  │ - 本地回放             │                                 │
│  └────────────────────────┘                                 │
└─────────────────────────────────────────────────────────────┘
```

#### 6.2 实现方案

```cpp
// src/gui/WaveformRecorder.cpp

void WaveformRecorderWidget::setRemoteDataSource(RemoteClient* client, 
                                                  const QString& channelName) {
    m_remoteClient = client;
    m_remoteChannelName = channelName;
    m_isRemoteDataSource = true;
    
    // 订阅远程数据
    if (m_remoteClient) {
        connect(m_remoteClient, &RemoteClient::realtimeDataReceived,
                this, &WaveformRecorderWidget::onRemoteDataReceived);
        m_remoteClient->subscribeToData();
    }
}

void WaveformRecorderWidget::onRemoteDataReceived(const QString& channelName,
                                                   const QJsonObject& data) {
    if (!m_isRemoteDataSource) return;
    if (channelName != m_remoteChannelName) return;
    if (m_state != RecorderState::Recording && 
        m_state != RecorderState::WaitingTrigger) return;
    
    // 从远程数据提取录波通道的值
    for (int i = 0; i < m_channels.size(); ++i) {
        const WaveformChannel& ch = m_channels[i];
        QString tagName = ch.tagName;
        
        if (data.contains(tagName)) {
            QJsonObject pointObj = data[tagName].toObject();
            double rawValue = pointObj["value"].toDouble();
            double scaledValue = ch.transform(rawValue);
            double timestamp = m_elapsedTimer.elapsed() / 1000.0;
            
            WaveformDataPoint dp(timestamp, rawValue, scaledValue);
            
            if (m_state == RecorderState::Recording) {
                m_channelData[i].append(dp);
            } else if (m_state == RecorderState::WaitingTrigger) {
                // 预触发缓存
                m_preTriggerBuffer[i].enqueue(dp);
                while (m_preTriggerBuffer[i].size() > m_preTriggerBufferSize) {
                    m_preTriggerBuffer[i].dequeue();
                }
            }
        }
    }
    
    // 更新图表
    if (m_state == RecorderState::Recording) {
        updatePlotGraphs();
    }
}
```

### 实施步骤

- [ ] 6.1 WaveformRecorder 支持远程数据源
- [ ] 6.2 实现远程数据接收和采样
- [ ] 6.3 远程模式下的变量选择器
- [ ] 6.4 录波数据本地保存
- [ ] 6.5 测试验证

---

## 🟡 R7: 通道类型区分（采集/服务）

### 需求描述
创建通道时可以选择类型：
- **采集通道**：只包含采集器，负责采集下行设备数据
- **服务/转发通道**：只包含服务器，从全局数据模型中选变量做映射

**注意**：不再保留混合通道类型。

### 技术方案

#### 7.1 修改 Channel.h

```cpp
// src/core/Channel.h

// 通道类型枚举（移除Hybrid）
enum class ChannelType {
    Collector,      // 采集通道：只包含采集器
    Server          // 服务/转发通道：只包含服务器
};

// 通道配置结构
struct ChannelConfig {
    QString name;
    ChannelType type = ChannelType::Collector;  // 默认采集通道
    bool enabled = true;
    QString description;
    QList<QJsonObject> collectors;      // 仅采集通道使用
    QList<QJsonObject> servers;         // 仅服务通道使用
};
```

#### 7.2 修改 ChannelConfigDialog

```cpp
// src/gui/ChannelConfigDialog.cpp

void ChannelConfigDialog::setupUi() {
    // 通道类型选择
    m_typeComboBox = new QComboBox(this);
    m_typeComboBox->addItem(tr("📥 采集通道"), static_cast<int>(ChannelType::Collector));
    m_typeComboBox->addItem(tr("📤 服务/转发通道"), static_cast<int>(ChannelType::Server));
    
    connect(m_typeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChannelConfigDialog::onTypeChanged);
}

void ChannelConfigDialog::onTypeChanged(int index) {
    ChannelType type = static_cast<ChannelType>(m_typeComboBox->currentData().toInt());
    
    // 根据类型显示/隐藏标签页
    if (type == ChannelType::Collector) {
        m_tabWidget->setTabVisible(m_collectorsTabIndex, true);
        m_tabWidget->setTabVisible(m_serversTabIndex, false);
    } else {
        m_tabWidget->setTabVisible(m_collectorsTabIndex, false);
        m_tabWidget->setTabVisible(m_serversTabIndex, true);
    }
}
```

### 实施步骤

- [ ] 7.1 修改 `ChannelType` 枚举（移除Hybrid）
- [ ] 7.2 修改 `ChannelConfig` 结构
- [ ] 7.3 修改 `Channel` 类支持类型验证
- [ ] 7.4 修改 `ChannelConfigDialog` 添加类型选择
- [ ] 7.5 根据类型显示/隐藏UI元素
- [ ] 7.6 配置文件兼容性处理
- [ ] 7.7 测试验证

---

## 🟡 R8: 全局数据模型（GlobalDataModel）

### 需求描述
创建全局数据模型，所有采集通道的数据都写入到全局模型，所有服务通道从全局模型读取数据。

### 技术方案

#### 8.1 GlobalDataModel 类

```cpp
// src/core/GlobalDataModel.h

#ifndef GLOBALDATAMODEL_H
#define GLOBALDATAMODEL_H

#include <QObject>
#include <QHash>
#include <QReadWriteLock>
#include "DataPoint.h"

namespace ModbusPlexLink {

class GlobalDataModel : public QObject {
    Q_OBJECT
    
public:
    static GlobalDataModel& instance();
    
    // 写入数据（采集通道调用）
    // fullTagName 格式: "通道名:采集器名:变量名"
    void updatePoint(const QString& channelName,
                    const QString& collectorName,
                    const QString& tagName,
                    const DataPoint& point);
    
    // 读取数据（服务通道调用）
    DataPoint readPoint(const QString& fullTagName) const;
    
    // 获取所有标签
    QStringList getAllTags() const;
    
    // 按通道获取标签
    QStringList getTagsByChannel(const QString& channelName) const;
    
    // 按采集器获取标签
    QStringList getTagsByCollector(const QString& channelName,
                                   const QString& collectorName) const;
    
    // 清除通道数据
    void clearChannelData(const QString& channelName);
    
signals:
    void dataUpdated(const QString& fullTagName, const DataPoint& point);
    void tagAdded(const QString& fullTagName);
    void tagRemoved(const QString& fullTagName);
    
private:
    GlobalDataModel();
    ~GlobalDataModel() = default;
    
    QString makeFullTagName(const QString& channelName,
                           const QString& collectorName,
                           const QString& tagName) const;
    
    QHash<QString, DataPoint> m_dataCache;
    mutable QReadWriteLock m_lock;
};

} // namespace ModbusPlexLink

#endif // GLOBALDATAMODEL_H
```

#### 8.2 数据流改造

```cpp
// 采集器数据写入全局模型
// src/core/Channel.cpp

void Channel::onCollectorDataUpdated(ICollector* collector,
                                     const QString& tagName,
                                     const DataPoint& point) {
    if (m_type == ChannelType::Collector) {
        // 写入全局数据模型
        GlobalDataModel::instance().updatePoint(
            m_name,                    // 通道名
            collector->getName(),      // 采集器名
            tagName,                   // 变量名
            point
        );
    }
}
```

```cpp
// 服务器从全局模型读取数据
// src/adapters/ModbusTcpServer.cpp

DataPoint VirtualizationEngine::readFromGlobalModel(const ServerMappingRule& rule) {
    QString fullTagName = QString("%1:%2:%3")
        .arg(rule.sourceChannel)
        .arg(rule.sourceCollector)
        .arg(rule.sourceTagName);
    
    return GlobalDataModel::instance().readPoint(fullTagName);
}
```

### 实施步骤

- [ ] 8.1 创建 `GlobalDataModel` 单例类
- [ ] 8.2 实现数据读写方法
- [ ] 8.3 修改采集器写入全局模型
- [ ] 8.4 修改服务器从全局模型读取
- [ ] 8.5 扩展 `ServerMappingRule` 支持 `sourceChannel`
- [ ] 8.6 测试验证

---

## 🟡 R9: 系统变量管理器

### 需求描述
整个系统公用系统变量，从所有采集通道的采集器映射中自动提取变量，供服务通道做映射时选择。

### 技术方案

#### 9.1 SystemVariableManager 类

```cpp
// src/utils/SystemVariableManager.h

struct SystemVariable {
    QString variableName;        // 变量名
    QString sourceChannel;       // 来源通道
    QString sourceCollector;     // 来源采集器
    QString fullId;              // 完整标识: 通道:采集器:变量名
    DataType dataType;           // 数据类型
    QString comment;             // 注释
    QString unit;                // 单位
    QDateTime updateTime;        // 更新时间
};

class SystemVariableManager : public QObject {
    Q_OBJECT
    
public:
    static SystemVariableManager& instance();
    
    // 从通道管理器同步变量
    void syncFromChannels(ChannelManager* channelManager);
    
    // 获取所有变量
    QList<SystemVariable> getAllVariables() const;
    
    // 按通道筛选
    QList<SystemVariable> getVariablesByChannel(const QString& channelName) const;
    
    // 按采集器筛选
    QList<SystemVariable> getVariablesByCollector(const QString& channelName,
                                                   const QString& collectorName) const;
    
    // 搜索变量
    QList<SystemVariable> searchVariables(const QString& keyword) const;
    
signals:
    void variablesUpdated();
    
private:
    SystemVariableManager();
    
    QMap<QString, SystemVariable> m_variables;
    mutable QMutex m_mutex;
};
```

#### 9.2 系统变量管理界面

```cpp
// src/gui/SystemVariableManagerDialog.h

class SystemVariableManagerDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SystemVariableManagerDialog(QWidget* parent = nullptr);
    
private slots:
    void onRefresh();
    void onExportCsv();
    void onFilterChanged();
    void onSearchTextChanged();
    
private:
    void setupUi();
    void refreshTable();
    
    QTableWidget* m_variableTable;
    QComboBox* m_channelFilter;
    QComboBox* m_collectorFilter;
    QLineEdit* m_searchEdit;
    QPushButton* m_refreshBtn;
    QPushButton* m_exportBtn;
};
```

### 实施步骤

- [ ] 9.1 创建 `SystemVariableManager` 单例类
- [ ] 9.2 实现从通道同步变量
- [ ] 9.3 实现变量查询接口
- [ ] 9.4 创建 `SystemVariableManagerDialog`
- [ ] 9.5 在主菜单添加入口
- [ ] 9.6 修改服务器配置对话框使用系统变量
- [ ] 9.7 测试验证

---

## 🟡 R10: 批量创建通道功能

### 需求描述
支持批量创建多个点表相同但IP不同的采集通道。例如100台设备，点表模板相同，只是IP地址不同。

### 技术方案

#### 10.1 批量创建向导

```cpp
// src/gui/BatchChannelWizard.h

class BatchChannelWizard : public QWizard {
    Q_OBJECT
    
public:
    explicit BatchChannelWizard(ChannelManager* channelManager, QWidget* parent = nullptr);
    
    // 获取创建结果
    int getCreatedCount() const;
    QStringList getCreatedChannelNames() const;
    
private:
    // 向导页面
    QWizardPage* createIntroPage();
    QWizardPage* createTemplatePage();      // 选择模板通道或配置点表
    QWizardPage* createDeviceListPage();    // 设备列表（IP地址）
    QWizardPage* createNamingPage();        // 通道命名规则
    QWizardPage* createConfirmPage();       // 确认创建
    QWizardPage* createResultPage();        // 创建结果
    
    ChannelManager* m_channelManager;
};
```

#### 10.2 设备列表页面

```cpp
// 设备列表输入方式
class DeviceListPage : public QWizardPage {
    // 方式1：手动输入
    // IP地址范围: 192.168.1.100 - 192.168.1.199
    // Unit ID范围: 1 - 100
    
    // 方式2：CSV导入
    // IP,UnitId,Port,Description
    // 192.168.1.100,1,502,设备1
    // 192.168.1.101,2,502,设备2
    
    // 方式3：IP段批量生成
    // 基础IP: 192.168.1.
    // 起始: 100
    // 结束: 199
    // 步长: 1
};
```

#### 10.3 命名规则页面

```cpp
// 通道命名规则
class NamingPage : public QWizardPage {
    // 命名模板: {prefix}_{index:03d}
    // 示例:
    //   前缀: PowerMeter
    //   起始索引: 1
    //   结果: PowerMeter_001, PowerMeter_002, ...
    
    // 或使用设备IP作为名称后缀:
    //   模板: {prefix}_{ip_last}
    //   结果: PowerMeter_100, PowerMeter_101, ...
};
```

#### 10.4 批量创建实现

```cpp
// src/gui/BatchChannelWizard.cpp

void BatchChannelWizard::createChannels() {
    // 获取模板配置
    QJsonObject templateConfig = getTemplateConfig();
    QJsonArray collectors = templateConfig["collectors"].toArray();
    
    // 获取设备列表
    QList<DeviceInfo> devices = getDeviceList();
    
    // 获取命名规则
    QString nameTemplate = getNamingTemplate();
    
    // 批量创建
    int successCount = 0;
    for (int i = 0; i < devices.size(); ++i) {
        const DeviceInfo& device = devices[i];
        
        // 生成通道名称
        QString channelName = generateChannelName(nameTemplate, i, device);
        
        // 修改采集器IP
        QJsonArray modifiedCollectors;
        for (const QJsonValue& collVal : collectors) {
            QJsonObject coll = collVal.toObject();
            coll["ip"] = device.ip;
            coll["port"] = device.port;
            coll["unitId"] = device.unitId;
            modifiedCollectors.append(coll);
        }
        
        // 创建通道配置
        QJsonObject channelConfig;
        channelConfig["name"] = channelName;
        channelConfig["type"] = "Collector";
        channelConfig["description"] = device.description;
        channelConfig["collectors"] = modifiedCollectors;
        
        // 创建通道
        if (m_channelManager->createChannel(channelConfig)) {
            successCount++;
            m_createdChannels.append(channelName);
        }
    }
    
    m_createdCount = successCount;
}
```

#### 10.5 CSV导入格式

```csv
# 设备列表CSV格式
IP,Port,UnitId,Description
192.168.1.100,502,1,电表1号
192.168.1.101,502,1,电表2号
192.168.1.102,502,1,电表3号
...
```

### 实施步骤

- [ ] 10.1 创建 `BatchChannelWizard` 向导框架
- [ ] 10.2 实现模板选择页面
- [ ] 10.3 实现设备列表页面（手动/CSV/批量生成）
- [ ] 10.4 实现命名规则页面
- [ ] 10.5 实现批量创建逻辑
- [ ] 10.6 添加进度显示和结果报告
- [ ] 10.7 在主窗口菜单添加入口
- [ ] 10.8 测试验证

---

## 📊 实施计划时间线

```
Week 1: P0 紧急修复
├── Day 1: R1 历史告警SQLite持久化
├── Day 2: R2 历史告警显示修复 + R3 查看录波
└── Day 3: 测试和Bug修复

Week 2: P1 远程模式改进
├── Day 1: R4 模式状态Banner
├── Day 2-3: R5 远程模式告警管理
└── Day 4-5: R6 远程模式录波

Week 3: P2 架构改进
├── Day 1-2: R7 通道类型区分
├── Day 3-4: R8 全局数据模型
└── Day 5: R9 系统变量管理器（Part 1）

Week 4: P2 功能完善
├── Day 1: R9 系统变量管理器（Part 2）
├── Day 2-3: R10 批量创建通道
└── Day 4-5: 集成测试和文档更新
```

---

## 📝 变更记录

| 日期 | 版本 | 变更内容 |
|------|------|----------|
| 2026-01-29 | 1.0.0 | 初始版本，包含所有需求分析和实施方案 |

---

## ✅ 验收检查清单

### P0 验收
- [ ] 历史告警数据持久化到SQLite
- [ ] 重启后历史告警数据保留
- [ ] 历史告警列表正确显示
- [ ] 选中有录波的历史告警可以查看和回放

### P1 验收
- [ ] 告警界面显示当前模式状态
- [ ] 录波界面显示当前模式状态
- [ ] 远程模式下可以查看/配置远程告警规则
- [ ] 远程模式下可以查看远程告警历史
- [ ] 远程模式下可以录波远程数据

### P2 验收
- [ ] 可以创建采集通道（只有采集器）
- [ ] 可以创建服务通道（只有服务器）
- [ ] 服务通道可以从多个采集通道选择变量
- [ ] 系统变量管理界面可以查看所有变量
- [ ] 批量创建通道向导正常工作
- [ ] 支持CSV导入设备列表
