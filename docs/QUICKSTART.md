# 快速入门指南 | Quick Start Guide

本指南帮助您在 5 分钟内完成 Modbus PlexLink 的安装和首次使用。

---

## 📥 下载与安装

### Windows

1. 从 [Releases](../../releases) 页面下载最新版本
2. 解压 `Modbus_PlexLink-x.x.x-Windows-x64.zip`
3. 运行 `Modbus_PlexLink.exe`

### Linux

```bash
# 下载
wget https://github.com/user/Modbus_PlexLink/releases/download/vX.X.X/Modbus_PlexLink-X.X.X-Linux-x64.tar.gz

# 解压
tar -xzvf Modbus_PlexLink-X.X.X-Linux-x64.tar.gz

# 运行
chmod +x Modbus_PlexLink
./Modbus_PlexLink
```

### 从源码编译

```bash
# 克隆仓库
git clone https://github.com/user/Modbus_PlexLink.git
cd Modbus_PlexLink

# 创建构建目录
mkdir build && cd build

# 配置（替换为您的 Qt 安装路径）
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.6.3/mingw_64

# 编译
cmake --build . --config Release

# 运行
./Modbus_PlexLink
```

---

## 🚀 5 分钟快速体验

### 步骤 1: 创建通道

1. 启动程序后，点击菜单 **文件 → 新建通道**
2. 输入通道名称，如 `TestChannel`
3. 点击 **确定**

### 步骤 2: 添加采集器

1. 在左侧通道列表中，右键点击刚创建的通道
2. 选择 **添加采集器**
3. 配置采集器：
   - **名称**: `Collector1`
   - **协议**: `Modbus TCP`
   - **IP 地址**: 输入您的 Modbus 设备 IP
   - **端口**: `502`
   - **从站地址**: `1`
   - **采集周期**: `1000` ms

### 步骤 3: 配置数据点映射

1. 在采集器配置对话框中，切换到 **映射配置** 标签
2. 点击 **添加** 按钮
3. 配置数据点：
   - **标签名**: `Temperature`
   - **寄存器类型**: `Holding Register`
   - **起始地址**: `0`
   - **数据类型**: `Int16`
   - **倍率**: `0.1`（如果设备值是温度×10）
   - **单位**: `°C`
4. 点击 **确定** 保存

### 步骤 4: 启动通道

1. 点击工具栏的 **启动** 按钮
2. 或者右键通道选择 **启动通道**

### 步骤 5: 查看数据

1. 在右侧 **数据仪表盘** 中查看实时数据
2. 点击 **数据监控** 标签查看所有数据点

---

## 🎯 常用功能

### 添加虚拟服务器

将采集到的数据对外提供 Modbus 服务：

1. 右键通道 → **添加服务器**
2. 配置监听端口，如 `1502`
3. 添加虚拟设备和地址映射
4. 启动通道后，其他设备可以连接 `<本机IP>:1502` 读取数据

### 配置告警

1. 菜单 **工具 → 告警管理**
2. 点击 **添加规则**
3. 选择数据点，设置阈值和触发条件
4. 告警触发时会在界面显示并可触发录波

### 波形录制

1. 在数据监控界面，选择要录制的数据点
2. 点击 **开始录波**
3. 录制完成后可导出 CSV 或保存图片

### 远程 API

启动远程 API 服务器：

1. 菜单 **工具 → 远程 API 设置**
2. 配置 HTTP 端口（默认 8080）和 WebSocket 端口（默认 8081）
3. 启动服务

然后可以通过 HTTP API 访问：

```bash
# 获取通道列表
curl http://localhost:8080/api/channels

# 读取数据
curl http://localhost:8080/api/channels/TestChannel/data
```

---

## 🖥️ 无头服务模式

在没有图形界面的服务器上运行：

```bash
# 基本启动
./Modbus_PlexLink_Service

# 指定配置文件
./Modbus_PlexLink_Service --config myconfig.json

# 启用认证
./Modbus_PlexLink_Service --auth admin:password

# 自动启动通道
./Modbus_PlexLink_Service --auto-start

# 完整示例
./Modbus_PlexLink_Service \
    --config config.json \
    --http-port 8080 \
    --ws-port 8081 \
    --auth admin:secret \
    --auto-start
```

---

## ❓ 常见问题

### 无法连接设备

1. 检查网络连接：`ping <设备IP>`
2. 确认端口是否正确（标准 Modbus TCP 端口是 502）
3. 检查防火墙设置
4. 确认从站地址正确

### 数据读取错误

1. 检查寄存器地址是否正确
2. 确认数据类型匹配
3. 尝试不同的字节序配置
4. 查看日志了解详细错误信息

### 程序启动失败

1. 确保所有 DLL 文件在同一目录
2. 检查 Qt 运行时是否正确
3. 查看系统事件日志

---

## 📚 更多资源

- [完整文档](../README.md)
- [API 文档](API.md)
- [配置示例](../examples/)
- [更新日志](../CHANGELOG.md)
