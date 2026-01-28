# windeployqt 失败原因分析

## 🔍 问题现象

```
找不到平台插件。
D:\a\Modbus_PlexLink\Modbus_PlexLink\build\Modbus_PlexLink.exe 64位，发布可执行文件
Direct dependencies: Qt6Core Qt6Gui Qt6Network Qt6PrintSupport Qt6WebSockets Qt6Widgets
待部署：Qt6Core Qt6Gui Qt6Network Qt6PrintSupport Qt6WebSockets Qt6Widgets
```

`windeployqt` 能够检测到依赖，但无法找到 platform plugin (`qwindows.dll`)。

## 📋 失败原因分析

### 1. **Qt 安装路径检测失败** ⭐⭐⭐⭐⭐

**最常见原因**：`windeployqt` 通过以下方式查找 Qt 安装路径：

1. **从可执行文件读取**：检查可执行文件链接的 Qt DLL 路径
2. **环境变量**：`QTDIR`, `QT_DIR`, `QT_PLUGIN_PATH`
3. **注册表**：Windows 注册表中的 Qt 安装信息（本地开发环境）
4. **相对路径推断**：从 `windeployqt.exe` 所在目录推断

**在 CI 环境中的问题**：
- GitHub Actions 是干净环境，没有注册表信息
- Qt 通过 `jurplel/install-qt-action` 安装到非标准路径：`D:\a\Modbus_PlexLink\Qt\6.5.3\mingw_64`
- 可执行文件可能没有正确链接 Qt 路径信息

### 2. **工作目录问题** ⭐⭐⭐⭐

`windeployqt` 需要：
- 在可执行文件所在目录运行
- 或者使用 `--dir` 参数指定部署目录
- 正确的工作目录才能找到相对路径的插件

**当前代码**：
```powershell
Push-Location build
& $windeployqt --dir . Modbus_PlexLink.exe
```

这应该是正确的，但可能 `--dir .` 在某些情况下不起作用。

### 3. **环境变量设置时机** ⭐⭐⭐

环境变量需要在运行 `windeployqt` **之前**设置，但可能：
- PowerShell 环境变量作用域问题
- 变量值包含特殊字符（如路径中的空格）
- 变量未正确传递到子进程

### 4. **Qt 版本/工具问题** ⭐⭐

- Qt 6.5.3 的 `windeployqt` 可能有 bug
- `windeployqt` 对 MinGW 版本的支持问题
- 工具本身在某些 CI 环境下不稳定

### 5. **插件路径解析失败** ⭐⭐⭐

即使设置了 `QT_PLUGIN_PATH`，`windeployqt` 可能：
- 无法正确解析相对路径
- 需要绝对路径
- 需要特定的路径格式

## 🔧 解决方案对比

### 方案 1：使用 `--qmldir` 参数（不适用）

```powershell
windeployqt --qmldir <qml_dir> <exe>
```

**问题**：本项目不使用 QML，此参数无效。

### 方案 2：使用 `--libdir` 参数

```powershell
windeployqt --libdir <qt_lib_dir> <exe>
```

**可能有效**：明确指定 Qt 库目录。

### 方案 3：使用绝对路径

```powershell
windeployqt --dir "D:\a\Modbus_PlexLink\Modbus_PlexLink\build" "D:\a\Modbus_PlexLink\Modbus_PlexLink\build\Modbus_PlexLink.exe"
```

**可能有效**：避免相对路径问题。

### 方案 4：手动复制（当前方案）✅

**优点**：
- 完全可控
- 不依赖 `windeployqt` 的内部逻辑
- 在 CI 环境中更可靠

**缺点**：
- 需要手动维护 DLL 列表
- 可能遗漏某些依赖

## 💡 推荐的改进方案

### 方案 A：增强 windeployqt 调用

```powershell
# 使用绝对路径和更多参数
$exePath = Resolve-Path "Modbus_PlexLink.exe"
$deployDir = (Get-Location).Path

& $windeployqt `
  --release `
  --compiler-runtime `
  --no-translations `
  --no-system-d3d-compiler `
  --no-opengl-sw `
  --dir $deployDir `
  --libdir $QtDir `
  $exePath
```

### 方案 B：完全手动部署（当前实现）✅

```powershell
# 1. 复制所有 Qt DLL
# 2. 复制 platform plugin
# 3. 复制其他插件（如需要）
```

**优点**：最可靠，不依赖 `windeployqt` 的内部逻辑。

## 🎯 为什么当前方案更好

1. **CI 环境兼容性**：GitHub Actions 环境干净，手动复制更可控
2. **可预测性**：明确知道复制了哪些文件
3. **错误处理**：可以精确控制失败情况
4. **性能**：避免 `windeployqt` 的复杂检测逻辑

## 📝 建议

**保持当前的手动部署方案**，因为：
- ✅ 已经在 CI 中验证可行
- ✅ 不依赖 `windeployqt` 的内部实现
- ✅ 更容易调试和维护
- ✅ 可以精确控制部署的文件

如果未来需要支持更多 Qt 模块或插件，只需扩展手动复制列表即可。
