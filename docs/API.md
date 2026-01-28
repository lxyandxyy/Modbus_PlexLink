# API 文档 | API Documentation

## 目录 | Table of Contents

- [HTTP REST API](#http-rest-api)
- [WebSocket API](#websocket-api)
- [错误处理](#错误处理--error-handling)

---

## HTTP REST API

基础地址 | Base URL: `http://<host>:8080/api`

### 认证 | Authentication

如果启用了认证，需要在请求头中添加 Basic Auth：

```
Authorization: Basic base64(username:password)
```

---

### 通道管理 | Channel Management

#### 获取所有通道

```http
GET /api/channels
```

**响应示例：**

```json
{
    "success": true,
    "data": [
        {
            "name": "Channel1",
            "enabled": true,
            "description": "主采集通道",
            "running": true,
            "collectors": [...],
            "servers": [...]
        }
    ]
}
```

#### 获取指定通道

```http
GET /api/channels/{name}
```

#### 创建通道

```http
POST /api/channels
Content-Type: application/json

{
    "name": "NewChannel",
    "enabled": true,
    "description": "新建通道"
}
```

#### 更新通道

```http
PUT /api/channels/{name}
Content-Type: application/json

{
    "description": "更新后的描述",
    "enabled": false
}
```

#### 删除通道

```http
DELETE /api/channels/{name}
```

#### 启动通道

```http
POST /api/channels/{name}/start
```

#### 停止通道

```http
POST /api/channels/{name}/stop
```

---

### 采集器管理 | Collector Management

#### 获取通道的采集器列表

```http
GET /api/channels/{channelName}/collectors
```

#### 添加采集器

```http
POST /api/channels/{channelName}/collectors
Content-Type: application/json

{
    "name": "NewCollector",
    "protocol": "modbus-tcp",
    "ip": "192.168.1.100",
    "port": 502,
    "unitId": 1,
    "pollRate": 1000,
    "mappings": [...]
}
```

#### 更新采集器

```http
PUT /api/channels/{channelName}/collectors/{collectorName}
Content-Type: application/json

{
    "pollRate": 500
}
```

#### 删除采集器

```http
DELETE /api/channels/{channelName}/collectors/{collectorName}
```

---

### 服务器管理 | Server Management

#### 获取通道的服务器列表

```http
GET /api/channels/{channelName}/servers
```

#### 添加服务器

```http
POST /api/channels/{channelName}/servers
Content-Type: application/json

{
    "name": "NewServer",
    "protocol": "modbus-tcp",
    "listenAddress": "0.0.0.0",
    "port": 1502,
    "virtualDevices": [...]
}
```

---

### 数据操作 | Data Operations

#### 获取通道所有数据

```http
GET /api/channels/{channelName}/data
```

**响应示例：**

```json
{
    "success": true,
    "data": {
        "Temperature": {
            "value": 25.5,
            "quality": "Good",
            "timestamp": "2026-01-27T12:00:00.000Z"
        },
        "Humidity": {
            "value": 60.2,
            "quality": "Good",
            "timestamp": "2026-01-27T12:00:00.000Z"
        }
    }
}
```

#### 获取指定数据点

```http
GET /api/channels/{channelName}/data/{tagName}
```

#### 写入数据点

```http
PUT /api/channels/{channelName}/data/{tagName}
Content-Type: application/json

{
    "value": 100
}
```

---

### 告警管理 | Alarm Management

#### 获取活动告警

```http
GET /api/alarms
```

**响应示例：**

```json
{
    "success": true,
    "alarms": [
        {
            "id": "alarm_001_1706356800000",
            "ruleId": "alarm_001",
            "ruleName": "温度过高告警",
            "channelName": "Channel1",
            "tagName": "Temperature",
            "value": 85.5,
            "priority": 2,
            "state": 0,
            "message": "温度超过80°C警戒值",
            "activeTime": "2026-01-27T12:00:00.000Z",
            "acknowledged": false
        }
    ],
    "count": 1
}
```

#### 确认告警

```http
POST /api/alarms/{alarmId}/ack
```

**响应示例：**

```json
{
    "success": true,
    "message": "Alarm acknowledged: alarm_001_1706356800000"
}
```

#### 清除告警

```http
POST /api/alarms/{alarmId}/clear
```

**响应示例：**

```json
{
    "success": true,
    "message": "Alarm cleared: alarm_001_1706356800000"
}
```

#### 获取告警历史

```http
GET /api/alarms/history?start=2026-01-01T00:00:00&end=2026-01-02T00:00:00
```

**查询参数：**
- `start` (可选): 开始时间，ISO 8601格式，默认最近7天
- `end` (可选): 结束时间，ISO 8601格式，默认当前时间

**响应示例：**

```json
{
    "success": true,
    "history": [
        {
            "id": "alarm_001_1706356800000",
            "ruleId": "alarm_001",
            "ruleName": "温度过高告警",
            "channelName": "Channel1",
            "tagName": "Temperature",
            "value": 85.5,
            "message": "温度超过80°C警戒值",
            "activeTime": "2026-01-27T12:00:00.000Z",
            "acknowledgedTime": "2026-01-27T12:05:00.000Z",
            "clearedTime": "2026-01-27T12:10:00.000Z",
            "acknowledgedBy": "operator"
        }
    ],
    "count": 1,
    "startTime": "2026-01-01T00:00:00.000Z",
    "endTime": "2026-01-02T00:00:00.000Z"
}
```

---

### 告警规则管理 | Alarm Rule Management

#### 获取所有告警规则

```http
GET /api/alarm-rules
```

**响应示例：**

```json
{
    "success": true,
    "rules": [
        {
            "id": "alarm_001",
            "name": "温度过高告警",
            "enabled": true,
            "channelName": "Channel1",
            "tagName": "Temperature",
            "type": 0,
            "priority": 2,
            "highLimit": 80.0,
            "lowLimit": 0.0,
            "highHighLimit": 100.0,
            "lowLowLimit": -10.0,
            "deadband": 1.0,
            "delaySeconds": 5,
            "message": "温度超过80°C警戒值",
            "recordingConfig": {
                "enabled": true,
                "preTriggerSeconds": 60.0,
                "postTriggerSeconds": 30.0,
                "sampleIntervalMs": 100,
                "recordTags": ["Temperature", "Pressure"],
                "autoExportCsv": true,
                "keepInMemory": true
            }
        }
    ],
    "count": 1
}
```

#### 获取指定告警规则

```http
GET /api/alarm-rules/{ruleId}
```

#### 添加告警规则

```http
POST /api/alarm-rules
Content-Type: application/json

{
    "name": "温度过高告警",
    "enabled": true,
    "channelName": "Channel1",
    "tagName": "Temperature",
    "type": 0,
    "priority": 2,
    "highLimit": 80.0,
    "delaySeconds": 5,
    "message": "温度超过80°C警戒值"
}
```

**响应示例：**

```json
{
    "success": true,
    "ruleId": "alarm_001",
    "message": "Alarm rule added: 温度过高告警"
}
```

#### 更新告警规则

```http
PUT /api/alarm-rules/{ruleId}
Content-Type: application/json

{
    "name": "温度过高告警（更新）",
    "highLimit": 85.0
}
```

#### 删除告警规则

```http
DELETE /api/alarm-rules/{ruleId}
```

#### 启用/禁用告警规则

```http
POST /api/alarm-rules/{ruleId}/enable
Content-Type: application/json

{
    "enabled": true
}
```

---

### 录波管理 | Recording Management

#### 获取录波数据列表

```http
GET /api/recording/data
```

**响应示例：**

```json
{
    "success": true,
    "recordings": [
        {
            "alarmEventId": "alarm_001_1706356800000",
            "alarmRuleName": "温度过高告警",
            "channelName": "Channel1",
            "tagName": "Temperature",
            "startTime": "2026-01-27T12:00:00.000Z",
            "endTime": "2026-01-27T12:01:30.000Z",
            "csvFilePath": "/recordings/alarm_001_1706356800000.csv",
            "recordedTags": ["Temperature", "Pressure"],
            "totalDataPoints": 900,
            "isComplete": true,
            "hasData": true
        }
    ],
    "count": 1
}
```

#### 获取录波回放数据

```http
POST /api/recording/playback
Content-Type: application/json

{
    "alarmEventId": "alarm_001_1706356800000"
}
```

**响应示例：**

```json
{
    "success": true,
    "alarmEventId": "alarm_001_1706356800000",
    "startTime": "2026-01-27T12:00:00.000Z",
    "endTime": "2026-01-27T12:01:30.000Z",
    "csvFilePath": "/recordings/alarm_001_1706356800000.csv",
    "recordedTags": ["Temperature", "Pressure"],
    "totalDataPoints": 900,
    "isComplete": true,
    "data": [
        {
            "timestamp": 1706356800000,
            "values": {
                "Temperature": 85.5,
                "Pressure": 1.2
            }
        },
        {
            "timestamp": 1706356800100,
            "values": {
                "Temperature": 85.6,
                "Pressure": 1.21
            }
        }
    ]
}
```

#### 导出录波数据

```http
POST /api/recording/export
Content-Type: application/json

{
    "alarmEventId": "alarm_001_1706356800000",
    "format": "csv",
    "filename": "recording_export.csv"
}
```

**响应示例：**

```json
{
    "success": true,
    "filename": "recording_export.csv",
    "message": "Recording exported to recording_export.csv"
}
```

**注意：** 独立录波功能（非告警触发的录波）在无头模式下暂未实现，建议使用告警自动录波功能。

---

### 系统信息 | System Information

#### 获取系统信息

```http
GET /api/system
```

**响应示例：**

```json
{
    "success": true,
    "data": {
        "version": "1.1.0",
        "platform": "Windows",
        "uptime": 3600,
        "channelCount": 3,
        "runningChannels": 2
    }
}
```

#### 获取统计信息

```http
GET /api/statistics
```

---

### 配置管理 | Configuration Management

#### 获取当前配置

```http
GET /api/config
```

#### 保存配置

```http
POST /api/config/save
```

#### 加载配置

```http
POST /api/config/load?file=config.json
```

---

## WebSocket API

连接地址 | Connection URL: `ws://<host>:8081`

### 消息格式 | Message Format

所有消息使用 JSON 格式：

```json
{
    "type": "message_type",
    "data": { ... }
}
```

### 订阅主题 | Subscribe to Topics

```json
{
    "type": "subscribe",
    "data": {
        "topics": ["data.Channel1", "alarms", "status"]
    }
}
```

**可用主题：**

| 主题 | 说明 |
|------|------|
| `data.<channel>` | 指定通道的数据更新 |
| `data.*` | 所有通道的数据更新 |
| `alarms` | 告警事件 |
| `status` | 系统状态变更 |

### 取消订阅 | Unsubscribe

```json
{
    "type": "unsubscribe",
    "data": {
        "topics": ["data.Channel1"]
    }
}
```

### 数据推送 | Data Push

服务器推送的数据更新消息：

```json
{
    "type": "data",
    "channel": "Channel1",
    "timestamp": "2026-01-27T12:00:00.000Z",
    "data": {
        "Temperature": {
            "value": 25.5,
            "quality": "Good",
            "timestamp": "2026-01-27T12:00:00.000Z"
        }
    }
}
```

### 告警推送 | Alarm Push

```json
{
    "type": "alarm",
    "event": "triggered",
    "data": {
        "id": "alarm_001_1706356800000",
        "ruleName": "温度过高告警",
        "severity": "warning",
        "message": "温度超过80°C警戒值",
        "timestamp": "2026-01-27T12:00:00.000Z"
    }
}
```

### 心跳 | Heartbeat

客户端可以发送心跳保持连接：

```json
{
    "type": "ping"
}
```

服务器响应：

```json
{
    "type": "pong",
    "timestamp": "2026-01-27T12:00:00.000Z"
}
```

---

## 错误处理 | Error Handling

### HTTP 错误响应

```json
{
    "success": false,
    "error": {
        "code": 404,
        "message": "Channel not found: NonExistent"
    }
}
```

### 常见错误码

| 状态码 | 说明 |
|--------|------|
| 400 | 请求参数错误 |
| 401 | 未授权（需要认证） |
| 404 | 资源不存在 |
| 409 | 资源冲突（如名称重复） |
| 500 | 服务器内部错误 |

### WebSocket 错误

```json
{
    "type": "error",
    "data": {
        "code": "INVALID_MESSAGE",
        "message": "Invalid message format"
    }
}
```

---

## 示例代码 | Code Examples

### Python

```python
import requests

base_url = "http://localhost:8080/api"

# 获取所有通道
response = requests.get(f"{base_url}/channels")
channels = response.json()

# 读取数据
response = requests.get(f"{base_url}/channels/Channel1/data/Temperature")
data = response.json()
print(f"Temperature: {data['data']['value']}")
```

### JavaScript

```javascript
// HTTP API
const response = await fetch('http://localhost:8080/api/channels');
const data = await response.json();

// WebSocket
const ws = new WebSocket('ws://localhost:8081');
ws.onopen = () => {
    ws.send(JSON.stringify({
        type: 'subscribe',
        data: { topics: ['data.Channel1'] }
    }));
};
ws.onmessage = (event) => {
    const message = JSON.parse(event.data);
    console.log('Received:', message);
};
```

### cURL

```bash
# 获取通道列表
curl http://localhost:8080/api/channels

# 启动通道
curl -X POST http://localhost:8080/api/channels/Channel1/start

# 写入数据
curl -X PUT http://localhost:8080/api/channels/Channel1/data/Temperature \
     -H "Content-Type: application/json" \
     -d '{"value": 25.5}'
```
