# 📦 设备模板库 | Device Templates

本目录包含预配置的设备模板，可快速添加常见 Modbus 设备。

## 📁 目录结构

```
templates/
├── power_meters/           # 电力仪表
│   ├── acrel_amc96.json   # 安科瑞 AMC96
│   ├── chint_ddsu666.json # 正泰 DDSU666
│   └── schneider_pm5300.json # 施耐德 PM5300
├── plc/                    # PLC
│   ├── siemens_s7_1200.json  # 西门子 S7-1200
│   └── schneider_m340.json   # 施耐德 M340
├── sensors/                # 传感器
│   └── temperature_sensor.json # 通用温度传感器
├── inverters/              # 变频器
├── template_schema.json    # 模板格式规范
└── README.md
```

## 🚀 使用方法

### 在 GUI 中使用

1. 打开采集器配置对话框
2. 点击 **"从模板加载"** 按钮
3. 选择设备类型和型号
4. 修改 IP 地址等连接参数
5. 确认添加

### 手动使用

1. 复制模板文件中的 `mappings` 部分
2. 粘贴到采集器配置的映射规则中
3. 根据实际设备调整参数

## 📝 模板格式

```json
{
    "templateInfo": {
        "id": "unique_id",
        "name": "设备名称",
        "manufacturer": "制造商",
        "model": "型号",
        "category": "power_meter|plc|sensor|inverter|other",
        "version": "1.0",
        "description": "设备描述"
    },
    "defaultConfig": {
        "protocol": "modbus-tcp",
        "port": 502,
        "unitId": 1,
        "pollRate": 1000,
        "byteOrder": "ABCD"
    },
    "mappings": [
        {
            "tagName": "标签名",
            "registerType": "Holding",
            "address": 0,
            "dataType": "Float32",
            "byteOrder": "CDAB",
            "scale": 1,
            "offset": 0,
            "unit": "V",
            "description": "描述",
            "group": "分组"
        }
    ],
    "documentation": {
        "manualUrl": "手册链接",
        "notes": "使用注意事项"
    }
}
```

## 🤝 贡献模板

欢迎贡献新的设备模板！请遵循以下步骤：

1. 复制 `template_schema.json` 了解格式要求
2. 创建新的 JSON 文件，放入对应目录
3. 填写完整的设备信息和映射配置
4. 测试验证后提交 Pull Request

### 命名规范

- 文件名：`厂商_型号.json`（小写，下划线分隔）
- 示例：`siemens_s7_1200.json`、`schneider_pm5300.json`

### 注意事项

- 寄存器地址需从设备手册获取
- 标注正确的字节序（常见厂商默认字节序见下表）
- 添加使用注意事项到 `documentation.notes`

## 📊 常见厂商字节序参考

| 厂商 | 常用字节序 | 说明 |
|------|------------|------|
| 西门子 | CDAB | 中端大序 |
| 施耐德 | CDAB / ABCD | 因产品线而异 |
| ABB | ABCD | 大端序 |
| 安科瑞 | ABCD | 大端序 |
| 三菱 | ABCD | 大端序 |
| 欧姆龙 | ABCD | 大端序 |
| 正泰 | AB / ABCD | 因产品而异 |

## 📋 已收录设备

### 电力仪表
- [x] 安科瑞 AMC96
- [x] 正泰 DDSU666
- [x] 施耐德 PM5300
- [ ] ABB M2M
- [ ] 威胜 DTSD341

### PLC
- [x] 西门子 S7-1200
- [x] 施耐德 M340
- [ ] 西门子 S7-1500
- [ ] 三菱 FX5U
- [ ] 欧姆龙 NX/NJ

### 传感器
- [x] 通用温度传感器
- [ ] 压力变送器
- [ ] 流量计

### 变频器
- [ ] ABB ACS880
- [ ] 西门子 G120
- [ ] 施耐德 ATV
