# 更新日志 | Changelog

本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/) 规范。

All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### 计划中 | Planned
- Modbus RTU over TCP 支持
- SQLite 数据持久化
- 设备配置模板库
- 主题切换（暗色/亮色）

---

## [1.1.0] - 2026-01-15

### 新增 | Added
- ✨ **远程 API 服务器**：HTTP REST API + WebSocket 实时推送
- ✨ **无头服务模式**：支持 `Modbus_PlexLink_Service` 后台运行
- ✨ **告警管理系统**：多级告警阈值、延时触发、自动确认
- ✨ **波形录波功能**：手动/触发录波、预触发缓存、CSV/图片导出
- ✨ **虚拟设备映射**：支持创建多个虚拟从站和灵活的地址映射
- ✨ **数据仪表盘**：实时数据卡片显示、趋势图
- ✨ **Modbus 报文查看器**：实时显示收发报文

### 优化 | Changed
- ⚡ 采集器连续地址优化合并读取，减少通信次数
- ⚡ 改进自动重连机制，支持配置重试次数和间隔
- 🎨 UI 界面优化，新增通道卡片式布局
- 📝 完善配置文件结构，支持更多选项

### 修复 | Fixed
- 🐛 修复 Float32 类型在 CDAB 字节序下解析错误
- 🐛 修复多客户端连接时的并发读写问题
- 🐛 修复通道停止后采集器未完全释放的问题

---

## [1.0.0] - 2025-10-01

### 新增 | Added
- 🎉 **首个正式版本发布**
- ✨ **Modbus TCP 采集器**：支持多设备并行采集
- ✨ **Modbus RTU 采集器**：支持串口通信
- ✨ **Modbus TCP 服务器**：虚拟化服务接口
- ✨ **通用数据模型 (UDM)**：统一数据缓存与交换
- ✨ **通道管理**：多通道独立运行
- ✨ **数据类型支持**：UInt16/Int16/UInt32/Int32/Float32/UInt64/Int64/Float64/Bool
- ✨ **字节序支持**：AB/BA/ABCD/DCBA/CDAB/BADC
- ✨ **倍率偏移转换**：Scale 和 Offset 配置
- ✨ **GUI 界面**：可视化配置和监控
- ✨ **配置导入导出**：JSON/CSV 格式支持

### 技术栈 | Tech Stack
- C++17 + Qt 6.6
- libmodbus 库
- 跨平台支持 (Windows/Linux)

---

## 版本说明 | Version Guide

- **主版本号 (Major)**：不兼容的 API 变更
- **次版本号 (Minor)**：向后兼容的功能新增
- **修订号 (Patch)**：向后兼容的 Bug 修复

### 版本标签 | Version Tags

| 标签 | 说明 |
|------|------|
| ✨ | 新功能 |
| ⚡ | 性能优化 |
| 🐛 | Bug 修复 |
| 🎨 | UI/UX 改进 |
| 📝 | 文档更新 |
| 🔧 | 配置相关 |
| 🎉 | 里程碑 |
| ⚠️ | 破坏性变更 |
| 🗑️ | 废弃功能 |

---

[Unreleased]: https://github.com/user/Modbus_PlexLink/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/user/Modbus_PlexLink/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/user/Modbus_PlexLink/releases/tag/v1.0.0
