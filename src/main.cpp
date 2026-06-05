// SPDX-License-Identifier: MIT
// BlackArch Launcher (Qt6/QML) — single binary, Rick & Morty themed.

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cstdio>

#include "backend.h"

namespace {

constexpr auto kWrapperBody = R"sh(#!/bin/sh
exec "%1" "$@"
)sh";

constexpr auto kDesktopBody = R"desktop([Desktop Entry]
Type=Application
Version=1.0
Name=BlackArch 工具启动器
GenericName=BlackArch Tool Launcher
Comment=Modern Qt6/QML launcher for BlackArch distrobox tools (Rick & Morty themed)
Exec=%1
Icon=%2
Terminal=false
Categories=Security;Utility;
Keywords=blackarch;security;pentest;launcher;
StartupNotify=true
)desktop";

bool writeFile(const QString& path, const QString& content, bool exec = false) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "无法写入" << path << ":" << f.errorString();
        return false;
    }
    f.write(content.toUtf8());
    f.close();
    if (exec) {
        QFile::setPermissions(path,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
            QFileDevice::ReadGroup | QFileDevice::ExeGroup |
            QFileDevice::ReadOther | QFileDevice::ExeOther);
    }
    return true;
}

// Find the bundled assets directory.  Checks in order:
//   1. Compile-time ASSETS_INSTALL_DIR (for system-wide installs via PKGBUILD / make install)
//   2. Relative to the binary — ../../share/blackarch-launcher (common prefix layouts)
//   3. Relative to the binary — ../assets  (development build in build/)
//   4. ../assets relative to CWD (development, launched from project root)
QString findAssetsDir(const QString& binaryPath) {
    // 1) Compile-time install prefix.
    const QString prefixPath = QString::fromUtf8(ASSETS_INSTALL_DIR);
    if (QDir(prefixPath).exists()) return prefixPath;

    // 2) Relative to binary: /usr/bin/ → /usr/share/blackarch-launcher/
    const QDir binDir = QFileInfo(binaryPath).absoluteDir();
    QString rel = binDir.absolutePath();
    // Walk up twice:  ../../share/blackarch-launcher
    QDir d(rel);
    d.cdUp(); d.cdUp();
    rel = d.absolutePath() + "/share/blackarch-launcher";
    if (QDir(rel).exists()) return rel;

    // 3) Relative to binary:  build/blackarch-launcher  →  ../assets/
    rel = binDir.absolutePath() + "/../assets";
    if (QDir(rel).exists()) return QDir(rel).absolutePath();

    // 4) Relative to CWD (launched from project root).
    if (QDir("./assets").exists()) return QFileInfo("./assets").absoluteFilePath();

    return {};
}

// Copy a directory tree, skipping files that already exist at the destination.
void copyAssetsIfMissing(const QString& srcRoot, const QString& dstRoot, const QString& sub) {
    const QString srcDir = srcRoot + "/" + sub;
    const QString dstDir = dstRoot + "/" + sub;
    if (!QDir(srcDir).exists()) return;

    QDir().mkpath(dstDir);
    const auto entries = QDir(srcDir).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& fi : entries) {
        if (fi.isDir()) {
            copyAssetsIfMissing(srcRoot, dstRoot, sub + "/" + fi.fileName());
            continue;
        }
        const QString dstPath = dstDir + "/" + fi.fileName();
        if (!QFile::exists(dstPath)) {
            QFile::copy(fi.absoluteFilePath(), dstPath);
        }
    }
}

int doInstall(const QString& binaryPath) {
    const auto home = QDir::homePath();
    const QString binDir   = home + "/.local/bin";
    const QString appsDir  = home + "/.local/share/applications";
    const QString iconDir  = home + "/.local/share/icons/blackarch-tools";
    const QString cacheDir = home + "/.cache/blackarch-launcher";
    const QString deskDir  = home + "/桌面";

    QDir().mkpath(binDir);
    QDir().mkpath(appsDir);
    QDir().mkpath(iconDir);
    QDir().mkpath(cacheDir);

    // ── Copy bundled assets to user dirs ──────────────────────────────
    const QString assetsRoot = findAssetsDir(binaryPath);
    if (!assetsRoot.isEmpty()) {
        std::fprintf(stdout, "Assets: %s\n", qUtf8Printable(assetsRoot));

        // Tool icons → ~/.local/share/icons/blackarch-tools/
        const QString srcIcons = assetsRoot + "/icons";
        if (QDir(srcIcons).exists()) {
            const auto files = QDir(srcIcons).entryInfoList({"*.svg", "*.png", "*.ico"}, QDir::Files);
            for (const auto& fi : files) {
                const QString dst = iconDir + "/" + fi.fileName();
                if (!QFile::exists(dst))
                    QFile::copy(fi.absoluteFilePath(), dst);
            }
            std::fprintf(stdout, "  icons   : %d 已安装\n", int(files.size()));
        }

        // Background images → ~/.cache/blackarch-launcher/
        copyAssetsIfMissing(assetsRoot, cacheDir, "backgrounds");
        copyAssetsIfMissing(assetsRoot, cacheDir, "backgrounds_compose");
        copyAssetsIfMissing(assetsRoot, cacheDir, "gif_corner");

        std::fprintf(stdout, "  backgrounds / compose / gif → %s\n", qUtf8Printable(cacheDir));
    } else {
        std::fprintf(stdout, "(未找到内置资源目录，跳过资源复制)\n");
    }

    // ── Wrapper script ─────────────────────────────────────────────────
    const QString wrapper = binDir + "/blackarch-tree";
    writeFile(wrapper, QString::fromUtf8(kWrapperBody).arg(binaryPath), /*exec=*/true);

    QString icon = iconDir + "/blackarch-tree-rick-64.png";
    if (!QFile::exists(icon)) icon = iconDir + "/blackarch-tree.svg";
    if (!QFile::exists(icon)) icon = "applications-system";

    const QString desktopBody = QString::fromUtf8(kDesktopBody).arg(wrapper, icon);
    writeFile(appsDir + "/blackarch-tree.desktop", desktopBody);

    if (QDir(deskDir).exists()) {
        const auto deskPath = deskDir + "/blackarch-tree.desktop";
        writeFile(deskPath, desktopBody, /*exec=*/true);
    }

    QDir().mkpath(home + "/.cache/blackarch-launcher");
    QDir().mkpath(home + "/.config/blackarch-launcher");

    std::fprintf(stdout, "\n安装完成：\n");
    std::fprintf(stdout, "  binary  : %s\n", qUtf8Printable(binaryPath));
    std::fprintf(stdout, "  wrapper : %s\n", qUtf8Printable(wrapper));
    std::fprintf(stdout, "  desktop : %s/blackarch-tree.desktop\n", qUtf8Printable(appsDir));
    if (QDir(deskDir).exists())
        std::fprintf(stdout, "  桌面    : %s/blackarch-tree.desktop\n", qUtf8Printable(deskDir));
    std::fprintf(stdout, "\n运行：blackarch-tree\n");
    return 0;
}

int doUninstall() {
    const auto home = QDir::homePath();
    const QStringList targets = {
        home + "/.local/bin/blackarch-tree",
        home + "/.local/share/applications/blackarch-tree.desktop",
        home + "/桌面/blackarch-tree.desktop",
    };
    for (const auto& p : targets) {
        if (QFile::exists(p)) {
            if (QFile::remove(p))
                std::fprintf(stdout, "已删除：%s\n", qUtf8Printable(p));
            else
                std::fprintf(stderr, "删除失败：%s\n", qUtf8Printable(p));
        }
    }
    std::fprintf(stdout, "（保留 cache/config）\n");
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("blackarch-launcher");
    QGuiApplication::setApplicationDisplayName("BlackArch 工具启动器");
    QGuiApplication::setOrganizationName("blackarch-launcher");

    QCommandLineParser parser;
    parser.setApplicationDescription("现代化 BlackArch 工具启动器（Rick & Morty 主题）");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption installOpt("install", "安装快捷方式与桌面入口");
    QCommandLineOption uninstallOpt("uninstall", "卸载快捷方式与桌面入口");
    parser.addOption(installOpt);
    parser.addOption(uninstallOpt);
    parser.process(app);

    if (parser.isSet(installOpt))   return doInstall(QCoreApplication::applicationFilePath());
    if (parser.isSet(uninstallOpt)) return doUninstall();

    QQuickStyle::setStyle("Basic");

    // Window icon (uses already-installed Rick avatar if present).
    const QString rick = QDir::homePath() + "/.local/share/icons/blackarch-tools/blackarch-tree-rick-64.png";
    if (QFile::exists(rick))
        QGuiApplication::setWindowIcon(QIcon(rick));

    Backend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Backend", &backend);
    engine.loadFromModule("BlackArch", "Main");

    if (engine.rootObjects().isEmpty()) return -1;
    return app.exec();
}
