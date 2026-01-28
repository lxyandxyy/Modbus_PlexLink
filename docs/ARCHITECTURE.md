# 系统架构 | System Architecture

## 概述 | Overview

Modbus PlexLink 采用分层架构设计，实现了采集层、数据层、服务层的清晰分离。

```
┌─────────────────────────────────────────────────────────────────────┐
│                          Modbus PlexLink                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────┐    ┌─────────────────────────────────────────┐    │
│  │   远程客户端   │←→ │           RemoteApiServer               │    │
│  │  (Web/App)   │    │    (HTTP REST + WebSocket)              │    │
│  └─────────────┘    └─────────────────────────────────────────┘    │
│                                    ↕                                │
│  ┌─────────────┐    ┌─────────────────────────────────────────┐    │
│  │  SCADA/HMI   │←→ │      ModbusTcpServer (虚拟化服务)        │    │
│  │  上位系统     │    │    ├── VirtualizationEngine            │    │
│  └─────────────┘    │    └── ServerMappingRules               │    │
│                      └─────────────────────────────────────────┘    │
│                                    ↕                                │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                   ChannelManager (通道管理器)                  │  │
│  │  ┌───────────┐  ┌───────────┐  ┌───────────┐                │  │
│  │  │ Channel 1 │  │ Channel 2 │  │ Channel N │                │  │
│  │  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘                │  │
│  └────────┼──────────────┼──────────────┼───────────────────────┘  │
│           ↓              ↓              ↓                          │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              UniversalDataModel (通用数据模型)                 │  │
│  │                     统一数据缓存与交换                         │  │
│  └──────────────────────────────────────────────────────────────┘  │
│           ↑              ↑              ↑                          │
│  ┌────────┴───┐  ┌───────┴───┐  ┌───────┴───┐                     │
│  │ TCP采集器   │  │ RTU采集器  │  │ TCP采集器  │                     │
│  └─────┬──────┘  └─────┬─────┘  └─────┬─────┘                     │
└────────┼───────────────┼──────────────┼─────────────────────────────┘
         ↓               ↓              ↓
   ┌──────────┐   ┌──────────┐   ┌──────────┐
   │ PLC设备   │   │ 电表设备  │   │ 传感器    │
   │(Modbus)   │   │(Modbus)  │   │(Modbus)  │
   └──────────┘   └──────────┘   └──────────┘
```

---

## 核心模块 | Core Modules

### 1. Core 层 (`src/core/`)

核心数据结构和基础类。

| 文件 | 说明 |
|------|------|
| `Channel.h/cpp` | 通道类，管理采集器和服务器的生命周期 |
| `ChannelManager.h/cpp` | 通道管理器，管理所有通道 |
| `UniversalDataModel.h/cpp` | 通用数据模型，统一数据缓存 |
| `DataPoint.h` | 数据点结构定义 |
| `DataTypes.h/cpp` | 数据类型定义和转换 |

#### UniversalDataModel (UDM)

UDM 是系统的数据中心，所有采集器将数据写入 UDM，所有服务器从 UDM 读取数据。

```cpp
class UniversalDataModel : public QObject {
public:
    // 写入数据
    void updatePoint(const QString& tagName, const DataPoint& point);
    
    // 读取数据
    DataPoint readPoint(const QString& tagName) const;
    
signals:
    void dataUpdated(const QString& tagName, const DataPoint& point);
};
```

### 2. Adapters 层 (`src/adapters/`)

协议适配器，实现具体的 Modbus 协议通信。

| 文件 | 说明 |
|------|------|
| `interfaces.h/cpp` | 采集器和服务器接口定义 |
| `ModbusTcpCollector.h/cpp` | Modbus TCP 采集器实现 |
| `ModbusRtuCollector.h/cpp` | Modbus RTU 采集器实现 |
| `ModbusTcpServer.h/cpp` | Modbus TCP 服务器实现 |

#### ICollector 接口

```cpp
class ICollector : public QObject {
public:
    virtual bool initialize(const QJsonObject& config) = 0;
    virtual void setDataModel(UniversalDataModel* udm) = 0;
    virtual void setMappingRules(const QList<CollectorMappingRule>& rules) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    
signals:
    void connectionStateChanged(bool connected);
    void dataCollected(const QString& tagName, const QVariant& value);
};
```

### 3. GUI 层 (`src/gui/`)

图形用户界面。

| 文件 | 说明 |
|------|------|
| `MainWindow.h/cpp` | 主窗口 |
| `ChannelCardWidget.h/cpp` | 通道卡片组件 |
| `CollectorConfigDialog.h/cpp` | 采集器配置对话框 |
| `ServerConfigDialog.h/cpp` | 服务器配置对话框 |
| `DataMonitorWidget.h/cpp` | 数据监控组件 |
| `DashboardDataWidget.h/cpp` | 数据仪表盘 |
| `AlarmWidget.h/cpp` | 告警显示组件 |
| `WaveformRecorder.h/cpp` | 波形录制器 |

### 4. Remote 层 (`src/remote/`)

远程 API 服务。

| 文件 | 说明 |
|------|------|
| `RemoteApiServer.h/cpp` | HTTP + WebSocket 服务器 |
| `RemoteClient.h/cpp` | 远程客户端（用于连接其他网关） |

### 5. Utils 层 (`src/utils/`)

工具类。

| 文件 | 说明 |
|------|------|
| `ConfigManager.h/cpp` | 配置管理，JSON 读写 |
| `AlarmManager.h/cpp` | 告警管理 |
| `CsvHelper.h/cpp` | CSV 导入导出 |

---

## 数据流 | Data Flow

### 采集流程

```
1. ModbusTcpCollector 定时轮询设备
2. 按照 MappingRules 读取寄存器
3. 数据类型转换和倍率/偏移计算
4. 调用 UDM.updatePoint() 更新数据
5. UDM 发出 dataUpdated 信号
6. GUI/Server/Alarm 等订阅者收到通知
```

### 服务流程

```
1. SCADA 客户端连接 ModbusTcpServer
2. 客户端发送 Modbus 读请求
3. Server 根据 VirtualDevice 配置找到映射
4. 从 UDM 读取对应 tagName 的数据
5. 应用倍率/偏移转换
6. 返回 Modbus 响应
```

---

## 线程模型 | Threading Model

```
┌─────────────────────────────────────────────────────────┐
│                      Main Thread                        │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐                 │
│  │   GUI   │  │  UDM    │  │ Signals │                 │
│  └─────────┘  └─────────┘  └─────────┘                 │
└─────────────────────────────────────────────────────────┘
         ↑              ↑              ↑
         │              │              │
┌────────┴────┐  ┌──────┴──────┐  ┌────┴────────┐
│ Collector   │  │ Collector   │  │   Server    │
│  Thread 1   │  │  Thread 2   │  │   Thread    │
└─────────────┘  └─────────────┘  └─────────────┘
```

- **主线程**: GUI 事件循环、UDM、信号槽
- **采集器线程**: 每个采集器在独立线程中运行
- **服务器线程**: 处理客户端连接和请求

UDM 使用 `QReadWriteLock` 保证线程安全。

---

## 扩展性设计 | Extensibility

### 添加新协议

1. 实现 `ICollector` 或 `IServer` 接口
2. 在 `IProtocolAdapter` 中注册
3. 更新 GUI 配置对话框

```cpp
class MyProtocolCollector : public ICollector {
    bool initialize(const QJsonObject& config) override;
    bool start() override;
    void stop() override;
    // ...
};
```

### 添加新数据类型

1. 在 `DataTypes.h` 中添加枚举值
2. 在 `DataTypes.cpp` 中实现转换函数
3. 更新 GUI 下拉选项

---

## 配置文件结构 | Configuration Structure

```json
{
    "version": "1.1.0",
    "channels": [
        {
            "name": "Channel1",
            "collectors": [
                {
                    "name": "Collector1",
                    "protocol": "modbus-tcp",
                    "mappings": [...]
                }
            ],
            "servers": [
                {
                    "name": "Server1",
                    "virtualDevices": [...]
                }
            ]
        }
    ]
}
```
