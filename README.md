# BlackArch Launcher (Qt6/QML)

现代化、视觉惊艳的 BlackArch distrobox 工具启动器。深色毛玻璃 + 多背景图滚动动画，Rick & Morty 主题。

![ui sketch](packaging/blackarch-launcher.svg)

## 特性

- **三栏布局** — 分类树 / 工具列表 / 详情面板，可拖动分隔
- **多背景图自动轮播** — `fade` / `slide` / `zoom` 三种过渡，间隔与时长可配
- **毛玻璃面板** — 预合成深色半透明背景图 + 实色面板叠加
- **75 个 BlackArch 工具** — 每个工具自带用法示例，选中即看
- **容器状态指示** — 每 8s 检测 `distrobox list`
- **启动历史** — 写入 `~/.cache/blackarch-launcher/history.json`
- **快捷键** — `Enter` 启动 / `Esc` 退出 / `Ctrl+F` 搜索 / `Ctrl+Shift+←/→` 切换背景
- **单一可执行文件** — QML 编译进二进制，分发只需一个 ELF

## 安装

### 方式一：PKGBUILD（Arch Linux / BlackArch）

```bash
# 从源码构建并安装
cd packaging/arch
makepkg -si
# 安装后初始化用户数据
blackarch-launcher --install
```

### 方式二：手动构建 + 本地安装

```bash
./install.sh --user
```

这等价于：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/blackarch-launcher --install
```

`--install` 会：
- 创建 `~/.local/bin/blackarch-tree` wrapper 脚本
- 安装 `.desktop` 文件到 `~/.local/share/applications/` 和桌面
- 复制内置资源（图标、背景、动效帧）到 `~/.cache/` 和 `~/.local/share/`

### 方式三：系统级安装

```bash
./install.sh --system
```

安装到 `/usr/local/bin` + `/usr/local/share/blackarch-launcher/`。

### 新设备快速搭建

在一台新 Arch 设备上：

```bash
# 1) 安装依赖
sudo pacman -S qt6-base qt6-declarative qt6-svg qt6-imageformats cmake gcc

# 2) 克隆仓库
git clone https://github.com/user/blackarch-launcher-qt.git
cd blackarch-launcher-qt

# 3) 构建 + 本地安装（包含所有资源）
./install.sh --user

# 4) 确保 BlackArch distrobox 已配置好（参考 blackarch-distrobox-setup.md）
# 5) 刷新 KDE 菜单缓存
kbuildsycoca6 --noincremental

# 6) 从系统菜单或终端启动
blackarch-tree
```

## 编译依赖

| 包名 | 用途 |
|------|------|
| `qt6-base` | Qt 核心库 |
| `qt6-declarative` | QML 引擎 |
| `qt6-svg` | SVG 图标渲染 |
| `qt6-imageformats` | AVIF/WebP 等额外图片格式 |
| `cmake` >= 3.20 | 构建系统 |
| `gcc` | C++20 编译器 |

## 配置

`~/.config/blackarch-launcher/config.json`，首次运行 `--install` 自动生成。

```json
{
  "paths": {
    "backgrounds":         "~/.cache/blackarch-launcher/backgrounds",
    "backgrounds_compose": "~/.cache/blackarch-launcher/backgrounds_compose",
    "icons":               "~/.local/share/icons/blackarch-tools",
    "gif_corner":          "~/.cache/blackarch-launcher/gif_corner"
  },
  "background_roll": {
    "enabled": true,
    "interval": 5,
    "transition_duration": 1.5,
    "transition_style": "slide",
    "use_compose": false
  },
  "terminal": "konsole",
  "container": "distrobox enter blackarch --"
}
```

| 字段 | 含义 |
|------|------|
| `interval` | 背景自动切换间隔（秒） |
| `transition_duration` | 过渡动画时长（秒） |
| `transition_style` | `fade` / `slide` / `zoom` |
| `use_compose` | true 用预合成毛玻璃图，false 用原图（更透亮） |

## 项目结构

```
.
├── CMakeLists.txt              # CMake 构建（含 install 规则）
├── install.sh                  # 一键构建 + 安装
├── LICENSE                     # MIT
├── README.md
├── assets/                     # 运行时资源（由 --install 复制到用户目录）
│   ├── backgrounds/            #   壁纸原图
│   ├── backgrounds_compose/    #   毛玻璃合成版（优先使用）
│   ├── gif_corner/             #   右下角 5 帧动效
│   ├── icons/                  #   77 个工具 + 启动器图标
│   ├── morty-bg.png
│   └── morty-corner.png
├── packaging/
│   ├── arch/PKGBUILD           # Arch Linux 打包脚本
│   ├── blackarch-launcher.desktop
│   └── blackarch-launcher.svg
├── src/
│   ├── main.cpp                # 入口、--install / --uninstall
│   ├── backend.{h,cpp}         # 配置/启动/容器状态/历史
│   ├── desktopparser.{h,cpp}   # .desktop 解析
│   └── toolmodel.{h,cpp}       # QAbstractItemModel 两阶树 + 搜索过滤
└── qml/
    ├── Main.qml
    ├── BackgroundCarousel.qml   # 多背景滚动（双图层 + NumberAnimation）
    ├── BackgroundManager.qml
    ├── FallbackBackground.qml
    ├── GlassPane.qml
    ├── TitleBar.qml
    ├── ToolTree.qml
    ├── DetailsPane.qml
    └── CornerSprite.qml        # 右下角 ping-pong 动效
```

## 卸载

```bash
./build/blackarch-launcher --uninstall
```

保留 `~/.cache/blackarch-launcher/` 和 `~/.config/blackarch-launcher/`。

## 技术栈

**C++20 + Qt 6.5 + QML**

QML 的 `NumberAnimation` + `Transform` 将背景过渡动画压缩到极小代码量。`QSortFilterProxyModel` 做分类 + 搜索过滤，零拷贝、即时响应。QML 通过 `qt_add_qml_module` 编译进二进制，分发只需单个 ELF 文件。

## 设计决策

- **不用运行时 blur** — Wayland 下不稳定；用预合成毛玻璃图 + 半透明面板叠加
- **半透明面板** — `rgba(12,16,26,0.62)` + 1px 半透明白边，视觉接近原生玻璃
- **distrobox enter 在 Exec= 中** — 启动器不重写命令，只 strip `%f` 等字段码
- **`QProcess::startDetached`** — 子进程脱离父进程，关闭启动器不影响工具运行

## 许可

MIT — 见 [LICENSE](LICENSE)

壁纸和 Rick & Morty 相关图像衍生自公开的粉丝作品和截图，仅供个人/教育用途。如果重新分发或打包，请替换为你明确拥有使用权的素材。
