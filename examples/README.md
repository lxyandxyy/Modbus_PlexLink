# 示例配置文件 | Example Configurations

本目录包含各种使用场景的配置示例。

This directory contains configuration examples for various use cases.

## 📁 文件列表 | File List

| 文件 | 说明 | 适用场景 |
|------|------|----------|
| `config_basic.json` | 基本配置示例 | 入门学习、功能测试 |
| `config_power_meter.json` | 电力仪表采集 | 电力监控、能源管理 |
| `config_siemens_plc.json` | 西门子 PLC 采集 | 工业自动化、MES集成 |
| `config_multi_channel.json` | 多通道配置 | 复杂场景、多设备汇聚 |
| `alarm_config_example.json` | 告警配置示例 | 告警规则配置参考 |

## 🚀 快速开始 | Quick Start

### 1. 复制配置文件

```bash
# Windows
copy examples\config_basic.json config.json

# Linux
cp examples/config_basic.json config.json
```

### 2. 修改连接参数

编辑 `config.json`，将 IP 地址改为实际设备地址：

```json
{
    "collectors": [
        {
            "ip": "192.168.1.100",  // 改为您的设备 IP
            "port": 502,
            "unitId": 1
        }
    ]
}
```

### 3. 启动程序

```bash
./Modbus_PlexLink
```

## 📖 配置说明 | Configuration Guide

### 采集器配置 | Collector Configuration

```json
{
    "name": "采集器名称",
    "protocol": "modbus-tcp",      // modbus-tcp 或 modbus-rtu
    "ip": "192.168.1.100",         // 设备 IP
    "port": 502,                   // 端口号
    "unitId": 1,                   // 从站地址
    "pollRate": 1000,              // 采集周期 (ms)
    "timeout": 3000,               // 超时时间 (ms)
    "maxRetries": 3,               // 重试次数
    "autoReconnect": true          // 自动重连
}
```

### 数据点映射 | Data Point Mapping

```json
{
    "tagName": "Temperature",      // 标签名称（唯一标识）
    "registerType": "Holding",     // Holding/Input/Coil/Discrete
    "address": 0,                  // 寄存器地址
    "dataType": "Float32",         // 数据类型
    "byteOrder": "CDAB",           // 字节序
    "scale": 0.1,                  // 倍率
    "offset": 0,                   // 偏移量
    "unit": "°C"                   // 单位
}
```

### 数据类型 | Data Types

| 类型 | 寄存器数 | 说明 |
|------|----------|------|
| Bool | 1 bit | 布尔值 |
| UInt16 | 1 | 无符号16位整数 |
| Int16 | 1 | 有符号16位整数 |
| UInt32 | 2 | 无符号32位整数 |
| Int32 | 2 | 有符号32位整数 |
| Float32 | 2 | 32位浮点数 |
| UInt64 | 4 | 无符号64位整数 |
| Int64 | 4 | 有符号64位整数 |
| Float64 | 4 | 64位浮点数 |

### 字节序 | Byte Order

| 字节序 | 说明 | 常见设备 |
|--------|------|----------|
| AB | 大端序 (2字节) | 标准 Modbus |
| BA | 小端序 (2字节) | 部分国产设备 |
| ABCD | 大端序 (4字节) | 标准 32位 |
| DCBA | 小端序 (4字节) | Intel 设备 |
| CDAB | 中端大序 (4字节) | 西门子 PLC |
| BADC | 中端小序 (4字节) | 部分仪表 |

## 🏭 设备模板 | Device Templates

`device_templates/` 目录包含常见设备的预配置模板：

- 即将添加更多设备模板...

欢迎贡献您使用的设备配置模板！

## ❓ 常见问题 | FAQ

### Q: 如何确定设备的字节序？

A: 可以通过以下方法：
1. 查阅设备手册
2. 使用 Modbus 调试工具读取已知值进行对比
3. 尝试不同字节序配置

### Q: 采集周期设置多少合适？

A: 取决于应用场景：
- 快速变化的过程数据：100-500ms
- 一般监控数据：1000ms
- 环境数据：5000-60000ms

### Q: 如何处理通信超时？

A: 建议：
1. 检查网络连接
2. 增加 timeout 值
3. 适当增加 maxRetries
4. 启用 autoReconnect
