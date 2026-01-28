# 通道类型设计方案（采集通道 vs 服务/转发通道）

## 📋 需求分析

### 核心想法
将通道分为两种类型：
1. **采集通道（Collector Channel）**：只负责从Modbus设备采集数据
2. **服务/转发通道（Server/Forward Channel）**：只负责提供Modbus服务和转发数据

### 优势
- ✅ **职责分离**：采集和服务独立管理，更清晰
- ✅ **数据共享**：一个采集通道的数据可以被多个服务通道使用
- ✅ **灵活配置**：采集和服务可以独立启动/停止
- ✅ **易于扩展**：未来可以添加更多通道类型

---

## 🏗️ 架构设计

### 1. 通道类型定义

```cpp
// src/core/Channel.h

// 通道类型枚举
enum class ChannelType {
    Collector,      // 采集通道：只包含采集器
    Server,         // 服务/转发通道：只包含服务器
    Hybrid          // 混合通道：同时包含采集器和服务器（兼容旧配置）
};

// 通道配置结构（扩展）
struct ChannelConfig {
    QString name;                       // 通道名称
    ChannelType type = ChannelType::Hybrid;  // 通道类型（默认混合，兼容旧配置）
    bool enabled = true;                // 是否启用
    QString description;                // 通道描述
    QList<QJsonObject> collectors;      // 采集器配置列表（仅采集通道和混合通道）
    QList<QJsonObject> servers;         // 服务器配置列表（仅服务通道和混合通道）
    QJsonObject virtualization;         // 虚拟化配置（兼容旧格式）
};
```

### 2. 全局数据模型（GlobalDataModel）

```cpp
// src/core/GlobalDataModel.h

namespace ModbusPlexLink {

/**
 * @brief 全局数据模型
 * 
 * 所有采集通道的数据都写入到这里
 * 所有服务通道从这里读取数据
 */
class GlobalDataModel : public QObject {
    Q_OBJECT
    
public:
    static GlobalDataModel& instance();
    
    // 写入/更新数据点（采集通道调用）
    void updatePoint(const QString& channelName,
                    const QString& collectorName,
                    const QString& tagName,
                    const DataPoint& point);
    
    // 读取数据点（服务通道调用）
    DataPoint readPoint(const QString& fullTagName) const;
    DataPoint readPoint(const QString& channelName,
                      const QString& collectorName,
                      const QString& tagName) const;
    
    // 获取所有标签
    QStringList getAllTags() const;
    
    // 按通道获取标签
    QStringList getTagsByChannel(const QString& channelName) const;
    
    // 按采集器获取标签
    QStringList getTagsByCollector(const QString& channelName,
                                   const QString& collectorName) const;
    
signals:
    void dataUpdated(const QString& fullTagName, const DataPoint& point);
    void tagAdded(const QString& fullTagName);
    void tagRemoved(const QString& fullTagName);
    
private:
    GlobalDataModel();
    ~GlobalDataModel() = default;
    
    // 生成完整标签名：通道名:采集器名:变量名
    QString makeFullTagName(const QString& channelName,
                           const QString& collectorName,
                           const QString& tagName) const;
    
    QHash<QString, DataPoint> m_dataCache;  // 数据缓存
    mutable QReadWriteLock m_lock;
};

} // namespace ModbusPlexLink
```

### 3. Channel 类改进

```cpp
// src/core/Channel.h (修改)

class Channel : public QObject {
    Q_OBJECT
    
public:
    // 获取通道类型
    ChannelType getType() const;
    
    // 设置通道类型
    void setType(ChannelType type);
    
    // 根据类型限制操作
    bool canAddCollector() const;  // 采集通道或混合通道可以添加采集器
    bool canAddServer() const;      // 服务通道或混合通道可以添加服务器
    
    // 获取UDM实例（仅用于混合通道的本地数据）
    UniversalDataModel* getLocalDataModel();
    
    // 获取全局数据模型引用（所有通道都可以访问）
    GlobalDataModel* getGlobalDataModel();
    
private:
    ChannelType m_type;  // 通道类型
    std::unique_ptr<UniversalDataModel> m_localUdm;  // 本地UDM（仅混合通道使用）
    
    // 根据通道类型决定数据写入位置
    void routeDataToModel(const QString& collectorName,
                         const QString& tagName,
                         const DataPoint& point);
};
```

### 4. 采集器适配（写入全局UDM）

```cpp
// src/core/Channel.cpp (修改)

void Channel::onCollectorDataUpdated(const QString& collectorName,
                                    const QString& tagName,
                                    const DataPoint& point) {
    if (m_type == ChannelType::Collector || m_type == ChannelType::Hybrid) {
        // 采集通道：写入全局UDM
        GlobalDataModel& globalUdm = GlobalDataModel::instance();
        globalUdm.updatePoint(m_name, collectorName, tagName, point);
        
        // 混合通道：同时写入本地UDM（兼容旧逻辑）
        if (m_type == ChannelType::Hybrid && m_localUdm) {
            m_localUdm->updatePoint(tagName, point);
        }
    }
}
```

### 5. 服务器适配（从全局UDM读取）

```cpp
// src/adapters/ModbusTcpServer.cpp (修改)

// VirtualizationEngine 需要从 GlobalDataModel 读取数据
class VirtualizationEngine {
    // 从全局UDM读取数据
    DataPoint readFromGlobalUdm(const QString& fullTagName) const {
        GlobalDataModel& globalUdm = GlobalDataModel::instance();
        return globalUdm.readPoint(fullTagName);
    }
    
    // 解析完整标签名：通道名:采集器名:变量名
    QString parseFullTagName(const ServerMappingRule& mapping) const {
        if (!mapping.sourceCollector.isEmpty()) {
            // 格式：通道名:采集器名:变量名
            return QString("%1:%2:%3")
                .arg(mapping.sourceChannel)
                .arg(mapping.sourceCollector)
                .arg(mapping.sourceTagName);
        }
        return mapping.sourceTagName;  // 兼容旧格式
    }
};
```

### 6. ServerMappingRule 扩展

```cpp
// src/core/DataTypes.h (修改)

struct ServerMappingRule {
    // 源变量配置（扩展）
    QString sourceChannel;      // 来源通道名称（新增）
    QString sourceCollector;     // 来源采集器名称
    QString sourceTagName;       // 源UDM标签名
    
    // 完整源标识：通道名:采集器名:变量名
    QString getFullSourceId() const {
        if (sourceChannel.isEmpty()) {
            // 兼容旧格式：采集器名:变量名
            if (sourceCollector.isEmpty()) {
                return sourceTagName;
            }
            return QString("%1:%2").arg(sourceCollector).arg(sourceTagName);
        }
        return QString("%1:%2:%3")
            .arg(sourceChannel)
            .arg(sourceCollector)
            .arg(sourceTagName);
    }
    
    // ... 其他字段保持不变
};
```

---

## 🎨 UI 改进

### 1. ChannelConfigDialog 改进

```cpp
// src/gui/ChannelConfigDialog.h (修改)

class ChannelConfigDialog : public QDialog {
    // 添加通道类型选择
    QComboBox* m_typeComboBox;  // 通道类型下拉框
    
private slots:
    void onTypeChanged(int index);  // 通道类型改变时的处理
    
private:
    void updateUIByType();  // 根据通道类型更新UI
};
```

**UI逻辑**：
- **采集通道**：只显示"采集器"标签页，隐藏"服务器"标签页
- **服务通道**：只显示"服务器"标签页，隐藏"采集器"标签页
- **混合通道**：显示所有标签页（兼容旧配置）

### 2. 服务器配置对话框改进

```cpp
// src/gui/VirtualDeviceConfigDialog.cpp (修改)

void VirtualDeviceConfigDialog::refreshVariableList() {
    // 从所有采集通道获取变量
    ChannelManager* channelMgr = getChannelManager();
    GlobalDataModel& globalUdm = GlobalDataModel::instance();
    
    m_availableVariables.clear();
    
    // 遍历所有通道
    for (Channel* channel : channelMgr->getAllChannels()) {
        if (channel->getType() != ChannelType::Collector &&
            channel->getType() != ChannelType::Hybrid) {
            continue;  // 跳过非采集通道
        }
        
        // 获取通道的所有采集器
        for (ICollector* collector : channel->getCollectors()) {
            // 从采集器配置中提取变量
            QJsonObject collectorConfig = getCollectorConfig(channel, collector);
            QJsonArray mappings = collectorConfig.value("mappings").toArray();
            
            for (const QJsonValue& mappingVal : mappings) {
                QJsonObject mapping = mappingVal.toObject();
                
                AvailableVariable var;
                var.channelName = channel->getName();        // 新增
                var.collectorName = collector->getName();
                var.tagName = mapping.value("tagName").toString();
                var.fullId = QString("%1:%2:%3")            // 格式：通道:采集器:变量
                    .arg(var.channelName)
                    .arg(var.collectorName)
                    .arg(var.tagName);
                var.dataType = DataTypeUtils::dataTypeFromString(
                    mapping.value("dataType").toString("UInt16"));
                var.comment = mapping.value("comment").toString();
                
                m_availableVariables.append(var);
            }
        }
    }
    
    updateVariableSelector();
}
```

### 3. 变量选择器UI改进

```cpp
// src/gui/VirtualDeviceConfigDialog.cpp

QWidget* VirtualDeviceConfigDialog::createVariableSelector() {
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    
    // 筛选区域
    QHBoxLayout* filterLayout = new QHBoxLayout();
    
    // 通道筛选
    QLabel* channelLabel = new QLabel(tr("来源通道:"), this);
    QComboBox* channelFilter = new QComboBox(this);
    channelFilter->addItem(tr("全部通道"), "");
    // 填充所有采集通道
    for (const QString& channelName : getCollectorChannelNames()) {
        channelFilter->addItem(channelName, channelName);
    }
    
    // 采集器筛选
    QLabel* collectorLabel = new QLabel(tr("来源采集器:"), this);
    QComboBox* collectorFilter = new QComboBox(this);
    collectorFilter->addItem(tr("全部采集器"), "");
    
    // 搜索框
    QLineEdit* searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(tr("搜索变量名或注释..."));
    
    filterLayout->addWidget(channelLabel);
    filterLayout->addWidget(channelFilter);
    filterLayout->addWidget(collectorLabel);
    filterLayout->addWidget(collectorFilter);
    filterLayout->addWidget(searchEdit);
    filterLayout->addStretch();
    
    layout->addLayout(filterLayout);
    
    // 变量列表（表格）
    QTableWidget* variableTable = new QTableWidget(this);
    variableTable->setColumnCount(5);
    variableTable->setHorizontalHeaderLabels({
        tr("变量名"), tr("来源通道"), tr("来源采集器"), tr("数据类型"), tr("注释")
    });
    
    layout->addWidget(variableTable);
    
    // 连接信号
    connect(channelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualDeviceConfigDialog::onFilterChanged);
    connect(collectorFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualDeviceConfigDialog::onFilterChanged);
    connect(searchEdit, &QLineEdit::textChanged,
            this, &VirtualDeviceConfigDialog::onSearchTextChanged);
    
    return container;
}
```

---

## 📊 数据流设计

```
┌─────────────────────────────────────────────────────────────┐
│                   采集通道（Channel1）                        │
│  ┌──────────────┐  ┌──────────────┐                        │
│  │ Collector1  │  │ Collector2  │                        │
│  │ (Modbus设备) │  │ (Modbus设备) │                        │
│  └──────┬───────┘  └──────┬───────┘                        │
│         │                  │                                │
│         └──────────┬───────┘                                │
│                    │ 写入数据                                │
│                    ▼                                        │
│         ┌──────────────────────┐                            │
│         │  GlobalDataModel    │                            │
│         │  (全局数据模型)      │                            │
│         │  - Channel1:Coll1:Tag1                           │
│         │  - Channel1:Coll1:Tag2                           │
│         │  - Channel1:Coll2:Tag1                           │
│         │  - Channel2:Coll1:Tag1                           │
│         └──────────┬───────────┘                            │
└────────────────────┼────────────────────────────────────────┘
                     │
                     │ 读取数据
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                   服务通道（ServerChannel1）                  │
│  ┌──────────────┐  ┌──────────────┐                        │
│  │ Server1      │  │ Server2     │                        │
│  │ (Modbus服务) │  │ (Modbus服务)│                        │
│  └──────┬───────┘  └──────┬───────┘                        │
│         │                  │                                │
│         └──────────┬───────┘                                │
│                    │ 映射规则                                │
│                    │ sourceChannel:Channel1                 │
│                    │ sourceCollector:Collector1             │
│                    │ sourceTagName:Tag1                     │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔄 迁移策略

### 兼容旧配置

```cpp
// Channel::configure() 中处理
bool Channel::configure(const ChannelConfig& config) {
    m_config = config;
    
    // 自动判断通道类型（兼容旧配置）
    if (m_config.type == ChannelType::Hybrid) {
        // 根据实际配置判断
        bool hasCollectors = !config.collectors.isEmpty();
        bool hasServers = !config.servers.isEmpty();
        
        if (hasCollectors && !hasServers) {
            m_type = ChannelType::Collector;
        } else if (!hasCollectors && hasServers) {
            m_type = ChannelType::Server;
        } else {
            m_type = ChannelType::Hybrid;  // 保持混合模式
        }
    } else {
        m_type = config.type;
    }
    
    // 验证配置是否符合类型要求
    if (m_type == ChannelType::Collector && !config.servers.isEmpty()) {
        qWarning() << "Collector channel cannot have servers";
        return false;
    }
    if (m_type == ChannelType::Server && !config.collectors.isEmpty()) {
        qWarning() << "Server channel cannot have collectors";
        return false;
    }
    
    // ... 继续配置
}
```

---

## 📝 实现步骤

### Phase 1: 核心架构
1. ✅ 添加 `ChannelType` 枚举
2. ✅ 扩展 `ChannelConfig` 结构
3. ✅ 创建 `GlobalDataModel` 单例
4. ✅ 修改 `Channel` 类支持类型

### Phase 2: 数据路由
1. ✅ 修改采集器数据写入逻辑（写入全局UDM）
2. ✅ 修改服务器数据读取逻辑（从全局UDM读取）
3. ✅ 扩展 `ServerMappingRule` 支持跨通道

### Phase 3: UI改进
1. ✅ 修改 `ChannelConfigDialog` 添加类型选择
2. ✅ 根据类型显示/隐藏标签页
3. ✅ 改进服务器配置对话框的变量选择器
4. ✅ 添加通道/采集器筛选功能

### Phase 4: 兼容性
1. ✅ 实现旧配置自动迁移
2. ✅ 保持混合通道模式（向后兼容）
3. ✅ 测试和验证

---

## 🎯 关键优势

1. **职责清晰**：采集和服务分离，易于理解和维护
2. **数据共享**：一个采集通道的数据可以被多个服务通道使用
3. **灵活配置**：可以独立管理采集和服务
4. **向后兼容**：支持混合通道模式，不影响现有配置
5. **易于扩展**：未来可以添加更多通道类型（如转发通道、计算通道等）

---

## ❓ 待确认问题

1. **混合通道是否保留？**
   - 建议：保留，用于向后兼容和简单场景

2. **服务通道是否可以有多个？**
   - 建议：可以，一个采集通道的数据可以被多个服务通道使用

3. **变量命名规则？**
   - 建议：`通道名:采集器名:变量名`（完整标识）

4. **旧配置迁移策略？**
   - 建议：自动判断类型，保持混合模式作为默认
