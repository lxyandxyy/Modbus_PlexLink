# 贡献指南 | Contributing Guide

[English](#english) | [中文](#中文)

---

## 中文

感谢您对 **Modbus PlexLink** 的关注！我们欢迎任何形式的贡献，包括但不限于：

- 🐛 报告 Bug
- 💡 提出新功能建议
- 📝 改进文档
- 🔧 提交代码修复或新功能
- 🌍 翻译和本地化
- 📦 贡献设备配置模板

### 开始之前

1. **阅读文档**：请先阅读 [README.md](README.md) 了解项目概况
2. **搜索已有 Issue**：在创建新 Issue 前，请先搜索是否已有相关问题
3. **遵守行为准则**：请阅读并遵守 [行为准则](CODE_OF_CONDUCT.md)

### 报告 Bug

提交 Bug 报告时，请尽量包含以下信息：

1. **环境信息**
   - 操作系统及版本（如 Windows 10 21H2）
   - Qt 版本（如 Qt 6.6.3）
   - 编译器版本（如 MinGW 11.2）

2. **复现步骤**
   - 清晰描述如何复现该问题
   - 如果可能，提供最小复现示例

3. **期望行为 vs 实际行为**
   - 您期望发生什么
   - 实际发生了什么

4. **日志和截图**
   - 相关的错误日志
   - 界面截图（如果适用）

### 功能建议

提出功能建议时，请说明：

1. **使用场景**：描述您需要这个功能的具体场景
2. **解决方案**：您建议的实现方式（如果有的话）
3. **替代方案**：是否考虑过其他解决方案

### 代码贡献

#### 开发环境设置

```bash
# 1. Fork 并克隆仓库
git clone https://github.com/YOUR_USERNAME/Modbus_PlexLink.git
cd Modbus_PlexLink

# 2. 添加上游仓库
git remote add upstream https://github.com/ORIGINAL_OWNER/Modbus_PlexLink.git

# 3. 创建开发分支
git checkout -b feature/your-feature-name

# 4. 安装依赖并构建
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.6.3/mingw_64
cmake --build .
```

#### 代码规范

请遵循以下代码规范：

1. **命名规范**
   - 类名：大驼峰命名法（`ChannelManager`）
   - 函数名：小驼峰命名法（`getChannelName()`）
   - 成员变量：`m_` 前缀（`m_channelName`）
   - 常量：全大写下划线分隔（`MAX_RETRY_COUNT`）

2. **代码格式**
   - 使用 4 空格缩进
   - 大括号另起一行
   - 每行不超过 120 个字符

3. **注释规范**
   - 类和公共方法使用 Doxygen 风格注释
   - 复杂逻辑添加行内注释
   - 使用中文或英文注释均可

```cpp
/**
 * @brief 采集器基类接口
 * 
 * 定义所有 Modbus 采集器的通用接口
 */
class ICollector : public QObject
{
    Q_OBJECT
    
public:
    /**
     * @brief 初始化采集器
     * @param config JSON 格式的配置对象
     * @return 初始化是否成功
     */
    virtual bool initialize(const QJsonObject& config) = 0;
};
```

4. **提交信息规范**

使用语义化提交信息：

```
<type>(<scope>): <subject>

<body>

<footer>
```

类型（type）包括：
- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码格式调整（不影响逻辑）
- `refactor`: 代码重构
- `perf`: 性能优化
- `test`: 测试相关
- `chore`: 构建/工具链相关

示例：
```
feat(collector): 添加 Modbus RTU over TCP 支持

- 实现 RTU over TCP 透传模式
- 添加帧间隔配置选项
- 更新配置对话框

Closes #123
```

#### Pull Request 流程

1. **确保代码通过测试**
   ```bash
   cd build
   ctest --output-on-failure
   ```

2. **更新文档**（如果需要）

3. **创建 Pull Request**
   - 描述您的更改
   - 关联相关的 Issue
   - 等待代码审查

4. **响应审查反馈**
   - 及时回复审查者的评论
   - 根据反馈进行必要的修改

### 贡献设备模板

如果您想贡献设备配置模板（如特定型号的 PLC、电表等），请：

1. 在 `examples/device_templates/` 目录下创建配置文件
2. 使用清晰的命名：`<厂商>_<型号>.json`
3. 包含完整的映射配置和注释
4. 在 PR 中说明设备信息和测试环境

### 获取帮助

如果您在贡献过程中遇到问题：

- 💬 在 [GitHub Discussions](../../discussions) 提问
- 📧 发送邮件至 contact@modbusplexlink.com
- 🐛 创建 Issue 并标记为 `question`

---

## English

Thank you for your interest in **Modbus PlexLink**! We welcome all forms of contributions, including but not limited to:

- 🐛 Reporting bugs
- 💡 Suggesting new features
- 📝 Improving documentation
- 🔧 Submitting code fixes or new features
- 🌍 Translation and localization
- 📦 Contributing device configuration templates

### Before You Start

1. **Read the documentation**: Please read [README.md](README.md) first
2. **Search existing Issues**: Before creating a new Issue, search for existing ones
3. **Follow the Code of Conduct**: Please read and follow our [Code of Conduct](CODE_OF_CONDUCT.md)

### Reporting Bugs

When submitting a bug report, please include:

1. **Environment Information**
   - OS and version (e.g., Windows 10 21H2)
   - Qt version (e.g., Qt 6.6.3)
   - Compiler version (e.g., MinGW 11.2)

2. **Steps to Reproduce**
   - Clear description of how to reproduce the issue
   - Minimal reproduction example if possible

3. **Expected vs Actual Behavior**
   - What you expected to happen
   - What actually happened

4. **Logs and Screenshots**
   - Relevant error logs
   - Screenshots (if applicable)

### Feature Requests

When suggesting a feature, please describe:

1. **Use Case**: The specific scenario where you need this feature
2. **Proposed Solution**: Your suggested implementation (if any)
3. **Alternatives**: Other solutions you've considered

### Code Contributions

#### Development Environment Setup

```bash
# 1. Fork and clone the repository
git clone https://github.com/YOUR_USERNAME/Modbus_PlexLink.git
cd Modbus_PlexLink

# 2. Add upstream remote
git remote add upstream https://github.com/ORIGINAL_OWNER/Modbus_PlexLink.git

# 3. Create a development branch
git checkout -b feature/your-feature-name

# 4. Install dependencies and build
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.6.3/mingw_64
cmake --build .
```

#### Coding Standards

Please follow these coding standards:

1. **Naming Conventions**
   - Class names: PascalCase (`ChannelManager`)
   - Function names: camelCase (`getChannelName()`)
   - Member variables: `m_` prefix (`m_channelName`)
   - Constants: UPPER_SNAKE_CASE (`MAX_RETRY_COUNT`)

2. **Code Formatting**
   - Use 4 spaces for indentation
   - Opening braces on new line
   - Maximum 120 characters per line

3. **Comments**
   - Use Doxygen-style comments for classes and public methods
   - Add inline comments for complex logic
   - Comments in English or Chinese are both acceptable

4. **Commit Messages**

Use semantic commit messages (see Chinese section for details).

#### Pull Request Process

1. **Ensure tests pass**
   ```bash
   cd build
   ctest --output-on-failure
   ```

2. **Update documentation** (if needed)

3. **Create a Pull Request**
   - Describe your changes
   - Link related Issues
   - Wait for code review

4. **Respond to review feedback**
   - Reply to reviewer comments promptly
   - Make necessary changes based on feedback

### Getting Help

If you encounter issues while contributing:

- 💬 Ask in [GitHub Discussions](../../discussions)
- 📧 Email us at contact@modbusplexlink.com
- 🐛 Create an Issue and label it as `question`

---

感谢您的贡献！ | Thank you for contributing! 🎉
