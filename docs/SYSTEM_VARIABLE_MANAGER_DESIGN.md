# 系统变量管理器设计方案

## 📋 需求分析

### 当前问题
1. 服务器映射时只能从当前通道的采集器中选择变量
2. 无法跨通道使用变量
3. 没有统一的变量管理界面
4. 用户无法创建自定义系统变量

### 目标
1. ✅ 服务器可以从多个通道的采集器中选择变量
2. ✅ 用户不能自己创建变量（变量只能从采集器映射中生成）
3. ✅ 系统级变量管理界面
4. ✅ 可以新增系统变量（但必须基于现有采集器映射）

---

## 🏗️ 架构设计

### 1. 系统变量管理器（SystemVariableManager）

#### 1.1 核心类设计

```cpp
// src/utils/SystemVariableManager.h

namespace ModbusPlexLink {

// 系统变量信息
struct SystemVariable {
    QString variableName;        // 变量名（tagName）
    QString sourceChannel;      // 来源通道名称
    QString sourceCollector;     // 来源采集器名称
    QString fullId;              // 完整标识：通道:采集器:变量名
    DataType dataType;           // 数据类型
    QString comment;             // 注释/描述
    QString unit;                // 单位
    bool isAuto;                 // 是否自动变量（从采集器映射自动提取）
    bool isManual;               // 是否手动创建
    QDateTime createTime;        // 创建时间
    QDateTime updateTime;        // 更新时间
};

class SystemVariableManager : public QObject {
    Q_OBJECT
    
public:
    static SystemVariableManager& instance();
    
    // 从 ChannelManager 同步变量（自动变量）
    void syncFromChannels(ChannelManager* channelManager);
    
    // 手动添加系统变量（必须指定来源通道和采集器）
    bool addManualVariable(const QString& variableName,
                          const QString& sourceChannel,
                          const QString& sourceCollector,
                          const QString& comment = QString());
    
    // 删除手动变量
    bool removeManualVariable(const QString& variableName);
    
    // 获取所有系统变量
    QList<SystemVariable> getAllVariables() const;
    
    // 按通道筛选
    QList<SystemVariable> getVariablesByChannel(const QString& channelName) const;
    
    // 按采集器筛选
    QList<SystemVariable> getVariablesByCollector(const QString& channelName, 
                                                   const QString& collectorName) const;
    
    // 搜索变量（按名称、注释）
    QList<SystemVariable> searchVariables(const QString& keyword) const;
    
    // 获取变量信息
    SystemVariable getVariable(const QString& variableName) const;
    
    // 检查变量是否存在
    bool hasVariable(const QString& variableName) const;
    
    // 保存/加载配置
    bool saveConfig(const QString& filename = "system_variables.json") const;
    bool loadConfig(const QString& filename = "system_variables.json");
    
signals:
    void variableAdded(const SystemVariable& variable);
    void variableRemoved(const QString& variableName);
    void variablesUpdated();
    
private:
    SystemVariableManager();
    ~SystemVariableManager() = default;
    
    // 从采集器映射提取变量
    void extractVariablesFromCollector(const QString& channelName,
                                       const QString& collectorName,
                                       const QJsonObject& collectorConfig);
    
    QMap<QString, SystemVariable> m_autoVariables;   // 自动变量（只读）
    QMap<QString, SystemVariable> m_manualVariables;  // 手动变量（可编辑）
    mutable QMutex m_mutex;
};

} // namespace ModbusPlexLink
```

#### 1.2 变量来源机制

**自动变量（Auto Variables）**：
- 从所有通道的采集器映射规则中自动提取
- 变量名 = 采集器映射中的 `tagName`
- 完整标识 = `通道名:采集器名:变量名`
- 只读，随采集器配置自动更新

**手动变量（Manual Variables）**：
- 用户手动创建，但必须指定：
  - 来源通道和采集器（从现有采集器映射中选择）
  - 变量名（必须与来源采集器映射中的 tagName 一致）
- 可以编辑注释、描述等信息
- 可以删除

---

### 2. 服务器映射改进

#### 2.1 VirtualDeviceConfigDialog 改进

```cpp
// 改进：从 SystemVariableManager 获取变量列表
void VirtualDeviceConfigDialog::refreshVariableList() {
    SystemVariableManager& varMgr = SystemVariableManager::instance();
    
    // 获取所有系统变量
    QList<SystemVariable> allVars = varMgr.getAllVariables();
    
    // 转换为 AvailableVariable 格式
    m_availableVariables.clear();
    for (const SystemVariable& sysVar : allVars) {
        AvailableVariable var;
        var.collectorName = sysVar.sourceCollector;
        var.tagName = sysVar.variableName;
        var.fullId = sysVar.fullId;  // 格式：通道:采集器:变量名
        var.dataType = sysVar.dataType;
        var.comment = sysVar.comment;
        m_availableVariables.append(var);
    }
    
    // 更新变量选择器（支持按通道、采集器筛选）
    updateVariableSelector();
}
```

#### 2.2 变量选择器UI改进

- 添加筛选功能：
  - 按通道筛选下拉框
  - 按采集器筛选下拉框
  - 搜索框（按变量名、注释搜索）
- 显示变量详细信息：
  - 变量名
  - 来源通道
  - 来源采集器
  - 数据类型
  - 注释/描述

---

### 3. 系统变量管理界面

#### 3.1 SystemVariableManagerDialog

```cpp
// src/gui/SystemVariableManagerDialog.h

class SystemVariableManagerDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SystemVariableManagerDialog(QWidget* parent = nullptr);
    
private slots:
    void onAddVariable();
    void onEditVariable();
    void onDeleteVariable();
    void onRefreshVariables();
    void onExportCsv();
    void onImportCsv();
    void onFilterChanged();
    void onSearchTextChanged();
    
private:
    void setupUi();
    void refreshVariableTable();
    void updateVariableButtons();
    
    QTableWidget* m_variableTable;
    QComboBox* m_channelFilter;
    QComboBox* m_collectorFilter;
    QLineEdit* m_searchEdit;
    QPushButton* m_addBtn;
    QPushButton* m_editBtn;
    QPushButton* m_deleteBtn;
    QPushButton* m_refreshBtn;
};
```

#### 3.2 界面功能

**变量列表显示**：
- 表格列：变量名、来源通道、来源采集器、数据类型、注释、类型（自动/手动）、操作
- 支持排序、筛选

**添加变量**：
- 弹出对话框选择：
  - 来源通道（下拉框）
  - 来源采集器（下拉框，根据通道动态更新）
  - 变量名（下拉框，从采集器映射中选择）
  - 注释/描述（可选）

**编辑变量**：
- 只能编辑手动变量
- 可以修改注释、描述等信息

**删除变量**：
- 只能删除手动变量
- 自动变量不可删除

**筛选和搜索**：
- 按通道筛选
- 按采集器筛选
- 按变量名/注释搜索

---

### 4. 数据流设计

```
┌─────────────────────────────────────────────────────────┐
│              ChannelManager (通道管理器)                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │ Channel1 │  │ Channel2 │  │ ChannelN │              │
│  │  UDM     │  │  UDM     │  │  UDM     │              │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘              │
│       │             │             │                     │
│       └─────────────┼─────────────┘                     │
│                     │                                    │
│         ┌───────────▼───────────┐                       │
│         │  Collector Mappings   │                       │
│         │  (tagName, dataType)   │                       │
│         └───────────┬───────────┘                       │
└─────────────────────┼─────────────────────────────────────┘
                      │
                      │ 同步提取
                      ▼
┌─────────────────────────────────────────────────────────┐
│        SystemVariableManager (系统变量管理器)            │
│  ┌──────────────────┐  ┌──────────────────┐           │
│  │  自动变量列表     │  │  手动变量列表     │           │
│  │  (只读，自动)     │  │  (可编辑)        │           │
│  └──────────────────┘  └──────────────────┘           │
└─────────────────────┬─────────────────────────────────────┘
                      │
                      │ 提供变量列表
                      ▼
┌─────────────────────────────────────────────────────────┐
│      ServerConfigDialog / VirtualDeviceConfigDialog     │
│  ┌──────────────────────────────────────────────┐       │
│  │  变量选择器（支持跨通道选择）                  │       │
│  │  - 按通道筛选                                 │       │
│  │  - 按采集器筛选                               │       │
│  │  - 搜索变量                                   │       │
│  └──────────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────┘
```

---

### 5. 配置文件设计

#### 5.1 system_variables.json

```json
{
  "version": "1.1.0",
  "autoVariables": {
    "Channel1:Collector1:Temperature": {
      "variableName": "Temperature",
      "sourceChannel": "Channel1",
      "sourceCollector": "Collector1",
      "dataType": "Float32",
      "comment": "温度传感器",
      "unit": "°C",
      "isAuto": true
    }
  },
  "manualVariables": {
    "Channel2:Collector2:Pressure": {
      "variableName": "Pressure",
      "sourceChannel": "Channel2",
      "sourceCollector": "Collector2",
      "dataType": "Float32",
      "comment": "手动添加的压力变量",
      "unit": "kPa",
      "isManual": true,
      "createTime": "2026-01-15T10:30:00",
      "updateTime": "2026-01-15T10:30:00"
    }
  }
}
```

---

### 6. 实现步骤

#### Phase 1: 核心管理器
1. ✅ 创建 `SystemVariableManager` 类
2. ✅ 实现从 `ChannelManager` 同步变量
3. ✅ 实现手动变量管理（添加/删除）
4. ✅ 实现变量查询接口（按通道、采集器、搜索）

#### Phase 2: 服务器映射改进
1. ✅ 修改 `VirtualDeviceConfigDialog`，从 `SystemVariableManager` 获取变量
2. ✅ 添加变量筛选UI（通道、采集器、搜索）
3. ✅ 更新变量选择器，支持跨通道选择

#### Phase 3: 管理界面
1. ✅ 创建 `SystemVariableManagerDialog`
2. ✅ 实现变量列表显示
3. ✅ 实现添加/编辑/删除功能
4. ✅ 实现筛选和搜索功能
5. ✅ 在主窗口菜单添加入口

#### Phase 4: 数据同步
1. ✅ 监听 `ChannelManager` 的通道变化信号
2. ✅ 自动同步变量（通道创建/删除/修改时）
3. ✅ 处理变量冲突（同名变量）

---

### 7. 关键设计决策

#### 7.1 变量命名规则
- **完整标识格式**：`通道名:采集器名:变量名`
- **唯一性**：同一通道同一采集器内的变量名必须唯一
- **跨通道**：不同通道可以有同名变量，通过完整标识区分

#### 7.2 变量来源限制
- **用户不能自由创建变量**：必须从现有采集器映射中选择
- **手动变量**：只是标记某个自动变量为"手动管理"，可以添加注释等元数据
- **自动变量**：完全由采集器映射决定，不可编辑

#### 7.3 数据同步策略
- **实时同步**：通道配置变化时立即同步变量
- **启动时同步**：应用启动时从所有通道提取变量
- **冲突处理**：同名变量保留第一个，后续添加时提示

---

### 8. UI/UX 设计要点

#### 8.1 变量选择器（服务器映射时）
- **筛选区域**：通道下拉框 + 采集器下拉框 + 搜索框
- **变量列表**：显示变量名、来源、类型、注释
- **快速选择**：双击或点击"添加"按钮

#### 8.2 系统变量管理界面
- **布局**：左侧筛选区，右侧变量列表
- **操作按钮**：添加、编辑、删除、刷新、导入/导出
- **状态标识**：自动变量显示"自动"标签，手动变量显示"手动"标签

---

### 9. API 扩展（可选）

如果需要通过 API 管理系统变量：

```cpp
// RemoteApiServer 新增端点
GET    /api/system-variables              // 获取所有系统变量
GET    /api/system-variables/{name}      // 获取指定变量
POST   /api/system-variables              // 添加手动变量
DELETE /api/system-variables/{name}       // 删除手动变量
GET    /api/system-variables/search       // 搜索变量
```

---

## 📝 总结

这个方案实现了：
1. ✅ **跨通道变量选择**：服务器可以从任意通道的采集器中选择变量
2. ✅ **系统级变量管理**：统一的变量管理界面
3. ✅ **变量来源控制**：用户不能自由创建变量，必须基于采集器映射
4. ✅ **自动同步机制**：变量自动从采集器映射中提取
5. ✅ **灵活筛选**：支持按通道、采集器、变量名筛选

**关键优势**：
- 保持数据一致性（变量必须来自采集器）
- 简化用户操作（自动提取，无需手动维护）
- 支持跨通道数据共享
- 清晰的变量来源追踪
