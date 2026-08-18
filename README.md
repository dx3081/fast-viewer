# Fast Viewer

Fast Viewer 是一个小巧、快速的 Windows 原生图片查看器。双击图片即可查看和翻页，关闭后回到资源管理器。无需配置文件、无后台服务、无遥测。

## 特点

- 极速打开图片，专注沉浸式查看
- 简单直观的鼠标 / 键盘操作
- 轻量便携，单个可执行文件
- 无后台服务、无托盘进程、无自动更新、无遥测

## 支持格式

- JPEG / JPG、PNG、BMP、TIFF / TIF、WebP（不支持 GIF / RAW）

WebP 通过 Windows 图片处理组件（WIC）解码，已在当前 Windows 10 开发环境验证；实际解码能力取决于系统自带的 WIC 编解码器。

## 操作

鼠标：

- 滚轮：放大 / 缩小（以指针为中心）
- 左键拖拽：放大超出视口后平移
- 双击：切换沉浸查看 / 普通窗口模式
- 右键：关闭
- 移到窗口底部：显示胶片条；在胶片条上滚轮：上一张 / 下一张；点击缩略图：跳转到该图片

键盘：

- ← / →：上一张 / 下一张
- Esc：关闭
- F11：切换沉浸查看 / 普通窗口模式
- 1：切换 100% / 适应窗口

## 安装

### Portable（便携版）

1. 解压 ZIP，直接运行 `fast_viewer.exe`，无需安装。

### Installer（安装版）

- 仅安装到当前用户目录（`LocalAppData\Programs`），无需管理员权限
- 注册为受支持格式的“打开方式”候选应用
- 不会强制替换你现有的默认图片查看器
- 注意：可执行文件未进行代码签名，首次运行时 SmartScreen 可能会提示

## 设置为默认图片查看器

在资源管理器中右键图片 → “打开方式” → “选择其他应用” → 选择 Fast Viewer；或在“设置 → 应用 → 默认应用”中按文件类型选择。是否设为默认完全由你决定，Fast Viewer 不会擅自更改。

## 系统与运行环境

- 主要在 Windows 10 环境下开发和测试
- 单个原生可执行文件，静态链接 MSVC 运行时，无需安装 VC++ 运行库
- 仅依赖 Windows 系统自带组件

## 隐私与后台行为

- 无遥测、无网络访问、无后台服务、无托盘进程、无更新守护进程
- 关闭后不留任何后台进程

## 作者 / 协作

- DMZ
- ChatGPT
- DSH (DeepSeek Harness)

## 许可证

许可证尚未确定。

---

# Fast Viewer — English

Fast Viewer is a small, fast native Windows image viewer. Double-click an image to view and navigate, then close and you are back in Explorer. No configuration file, no background services, no telemetry.

## Features

- Fast image opening with a distraction-free immersive view
- Simple mouse and keyboard controls
- Lightweight and portable: a single executable
- No background services, no tray process, no updater daemon, no telemetry

## Supported Formats

- JPEG / JPG, PNG, BMP, TIFF / TIF, WebP (GIF / RAW are not supported)

WebP is decoded through the Windows Imaging Component (WIC) and has been verified on the current Windows 10 development environment; decoding availability depends on the WIC codecs present on the system.

## Controls

Mouse:

- Mouse wheel: zoom in / out (centered on the pointer)
- Left drag: pan when zoomed beyond the viewport
- Double-click: toggle immersive / normal window mode
- Right-click: close
- Move the pointer to the bottom edge: show the filmstrip; wheel over the filmstrip: previous / next image; click a thumbnail: jump to that image

Keyboard:

- Left / Right arrow: previous / next image
- Esc: close
- F11: toggle immersive / normal window mode
- 1: toggle 100% / fit view

## Installation

### Portable

1. Extract the ZIP and run `fast_viewer.exe`. No installation required.

### Installer

- Per-user installation under `LocalAppData\Programs`; no administrator rights required
- Registers Fast Viewer as an available “Open with” application for the supported formats
- Never forcibly replaces your existing default image viewer
- Note: the executable is not code-signed; SmartScreen may show a warning on first run

## Setting Fast Viewer as the Default Image Viewer

Right-click an image in Explorer → **Open with** → **Choose another app** → select Fast Viewer; or choose it per file type under **Settings → Apps → Default apps**. Whether it becomes your default is entirely your choice; Fast Viewer never changes that on its own.

## System / Runtime

- Primarily developed and tested on Windows 10
- Single native executable, statically linked MSVC runtime — no VC Redistributable required
- Depends only on Windows inbox/system components

## Privacy / Background Behavior

- No telemetry, no network access, no background service, no tray process, no updater daemon
- Nothing keeps running after the viewer is closed

## Authors / Collaboration

- DMZ
- ChatGPT
- DSH (DeepSeek Harness)

## License

License not yet selected.
