#!/usr/bin/env bash
# Build + user-local install helper for blackarch-launcher.
#
# Usage:
#   ./install.sh              Build only
#   ./install.sh --user       Build + install to ~/.local (wrapper + desktop + assets)
#   ./install.sh --system     Build + install to /usr/local  (requires sudo)
#   ./install.sh --help       Show this message
set -euo pipefail

cd "$(dirname "$0")"

usage() {
    sed -n '2,8p' "$0"
    exit 0
}

MODE="build"
case "${1:-}" in
    --user)   MODE="user" ;;
    --system) MODE="system" ;;
    --help)   usage ;;
    "")       ;;
    *) echo "未知参数: $1"; usage ;;
esac

# ── Check dependencies ────────────────────────────────────────────────────
missing=()
for dep in cmake gcc pkg-config; do
    command -v "$dep" >/dev/null 2>&1 || missing+=("$dep")
done

if ! pkg-config --exists Qt6Quick 2>/dev/null; then
    echo "需要 Qt6 开发包："
    echo "  sudo pacman -S qt6-base qt6-declarative qt6-svg qt6-imageformats qt6-tools"
    exit 1
fi

if [[ ${#missing[@]} -gt 0 ]]; then
    echo "缺少依赖: ${missing[*]}"
    echo "  sudo pacman -S cmake gcc pkgconf"
    exit 1
fi

# ── Build ─────────────────────────────────────────────────────────────────
echo "==> 构建..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

BIN="$(pwd)/build/blackarch-launcher"
echo "==> 构建完成: $BIN"

# ── Install ───────────────────────────────────────────────────────────────
case "$MODE" in
    user)
        echo "==> 用户本地安装..."
        "$BIN" --install
        ;;
    system)
        echo "==> 系统安装到 /usr/local ..."
        sudo cmake --install build
        echo "==> 运行 'blackarch-launcher --install' 来初始化用户数据"
        ;;
esac

echo
echo "启动: blackarch-tree"
echo "   或: $BIN"
