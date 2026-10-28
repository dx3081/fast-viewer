# Fast Viewer

Fast Viewer 是一个小巧、快速的 Windows 原生图片查看器。双击图片即可查看和翻页，关闭后回到资源管理器。无需配置文件、无后台服务、无遥测。

## 特点

- 极速打开图片，专注沉浸式查看
- 简单直观的鼠标 / 键盘操作
- 轻量便携，单个可执行文件
- 无后台服务、无托盘进程、无自动更新、无遥测

## 支持格式

- JPEG / JPG、PNG、BMP、TIFF / TIF、WebP、GIF（动图会播放）
- HEIC / HEIF（苹果手机照片）、AVIF、相机 RAW（.dng .cr2 .cr3 .nef .arw 等）：需要电脑上装了对应的 Windows 扩展（微软商店）。没装时会提示需要哪个扩展。
- RAW 平时显示相机自带的预览图，秒开；放大看细节时才解 RAW 原片。

能打开哪些格式由 Windows 图片处理组件（WIC）决定：装了新的系统解码器，Fast Viewer 不用更新就能用。

## 操作

鼠标：

- 滚轮：放大 / 缩小（以指针为中心）
- 左键拖拽：放大超出视口后平移
- 双击：切换沉浸查看 / 普通窗口模式
- 右键：关闭
- 移到窗口底部：显示底栏（胶片条 + 按钮）；在胶片条上滚轮：上一张 / 下一张；点击缩略图：跳转到该图片
- 底栏按钮：左边 放大 / 缩小 / 1:1，右边 向左转 / 向右转，缩略图两侧 上一张 / 下一张

键盘：

- ← / →：上一张 / 下一张
- Esc：关闭
- F11：切换沉浸查看 / 普通窗口模式
- 1：切换 100% / 适应窗口
- R / Shift+R：向右转 / 向左转（只转显示，不改文件；换图后复原）
- Ctrl+C：复制当前图片（可直接粘贴到聊天、文档；在资源管理器里粘贴得到文件）

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
- Codex
- DSH (DeepSeek Harness)

## 许可证

Copyright (C) 2026 DMZ

本项目依据 GNU General Public License v3.0 授权，仅适用第 3 版
（`GPL-3.0-only`）。完整条款见 [LICENSE](LICENSE)。

---

# Fast Viewer — English

Fast Viewer is a small, fast native Windows image viewer. Double-click an image to view and navigate, then close and you are back in Explorer. No configuration file, no background services, no telemetry.

## Features

- Fast image opening with a distraction-free immersive view
- Simple mouse and keyboard controls
- Lightweight and portable: a single executable
- No background services, no tray process, no updater daemon, no telemetry

## Supported Formats

- JPEG / JPG, PNG, BMP, TIFF / TIF, WebP, GIF (animations play)
- HEIC / HEIF, AVIF, camera RAW (.dng .cr2 .cr3 .nef .arw ...): when the matching Windows (Microsoft Store) extension is installed. If it is missing, the viewer says which extension is needed.
- RAW files show the camera's embedded preview instantly; zooming in develops the RAW data.

Supported formats come from the Windows Imaging Component (WIC): a newly installed system codec works without updating Fast Viewer.

## Controls

Mouse:

- Mouse wheel: zoom in / out (centered on the pointer)
- Left drag: pan when zoomed beyond the viewport
- Double-click: toggle immersive / normal window mode
- Right-click: close
- Move the pointer to the bottom edge: show the bottom bar (filmstrip + buttons); wheel over the filmstrip: previous / next image; click a thumbnail: jump to that image
- Bottom bar buttons: zoom in / zoom out / 1:1 on the left, rotate left / right on the right, previous / next beside the thumbnails

Keyboard:

- Left / Right arrow: previous / next image
- Esc: close
- F11: toggle immersive / normal window mode
- 1: toggle 100% / fit view
- R / Shift+R: rotate right / left (view only, the file is never changed; resets per image)
- Ctrl+C: copy the image (paste into chats/documents; paste in Explorer gives the file)

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
- Codex
- DSH (DeepSeek Harness)

## License

Copyright (C) 2026 DMZ

This project is licensed under the GNU General Public License version 3 only
(`GPL-3.0-only`). See [LICENSE](LICENSE) for the full license text.
