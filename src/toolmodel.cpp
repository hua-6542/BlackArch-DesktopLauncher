// SPDX-License-Identifier: MIT
#include "toolmodel.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QUrl>

namespace {

struct CatRule {
    const char* tag;
    const char* label;
    const char* emoji;
    const char* color;
    QStringList kws;
};

// BlackArch's .desktop files use these exact category tags:
//   X-BlackArch-{Recon,Web,Exploit,Cred,Reverse,Pwn,Forensic,Mobile,Traffic,Crypto}
// plus X-BlackArch-{CLI,GUI} as a flag for terminal vs graphical tools.
// Order matters: a tool matching multiple categories goes to the first hit.
static const QList<CatRule> kRules = {
    {"Recon",    "信息收集 / Recon",     "🛰",  "#5b8def", {"X-BlackArch-Recon"}},
    {"Web",      "Web 测试",             "🕸",  "#7a4eb8", {"X-BlackArch-Web"}},
    {"Exploit",  "攻防 / 内网",          "⚔",   "#d9534f", {"X-BlackArch-Exploit"}},
    {"Cred",     "凭据 / 密码",          "🔑",  "#f5b041", {"X-BlackArch-Cred"}},
    {"Reverse",  "逆向",                 "🧬",  "#3da556", {"X-BlackArch-Reverse"}},
    {"Pwn",      "Pwn",                  "💥",  "#ec7063", {"X-BlackArch-Pwn"}},
    {"Forensic", "取证",                 "🔬",  "#16a085", {"X-BlackArch-Forensic"}},
    {"Mobile",   "移动端",               "📱",  "#9b6dff", {"X-BlackArch-Mobile"}},
    {"Traffic",  "流量 / 抓包",          "📡",  "#1abc9c", {"X-BlackArch-Traffic"}},
    {"Crypto",   "Crypto",               "🔐",  "#a87c50", {"X-BlackArch-Crypto"}},
    {"Other",    "其它",                 "🧩",  "#888888", {}},
};

QString classifyTag(const QStringList& cats) {
    QSet<QString> set(cats.begin(), cats.end());
    for (const auto& rule : kRules) {
        for (const auto& k : rule.kws) {
            if (set.contains(k)) return QString::fromUtf8(rule.tag);
        }
    }
    return "Other";
}

QUrl iconForEntry(const DesktopEntry& e) {
    if (!e.icon.isEmpty() && QFileInfo::exists(e.icon))
        return QUrl::fromLocalFile(e.icon);
    const QString iconsDir = QDir::homePath() + "/.local/share/icons/blackarch-tools/";
    for (const auto& ext : {".svg", ".png"}) {
        const auto p = iconsDir + e.name + ext;
        if (QFileInfo::exists(p)) return QUrl::fromLocalFile(p);
    }
    return {};
}

// Tag id is encoded in the QModelIndex internalId:
//   id = 0 → category row (top-level)
//   id = (categoryRow + 1) → leaf row (parent is that category)
constexpr quintptr kCategoryId = 0;

} // namespace

ToolTreeModel::ToolTreeModel(QObject* parent) : QAbstractItemModel(parent) {}

void ToolTreeModel::setEntries(QList<DesktopEntry> entries) {
    beginResetModel();
    m_entries = std::move(entries);
    rebuild();
    endResetModel();
}

bool ToolTreeModel::entryMatches(const DesktopEntry& e) const {
    if (m_query.isEmpty()) return true;
    const auto q = m_query.toLower();
    return e.name.toLower().contains(q)
        || e.genericName.toLower().contains(q)
        || e.comment.toLower().contains(q);
}

void ToolTreeModel::rebuild() {
    m_cats.clear();
    QHash<QString, int> catIndex; // tag → index into m_cats

    for (const auto& rule : kRules) {
        Category c;
        c.tag = QString::fromUtf8(rule.tag);
        c.label = QString::fromUtf8(rule.label);
        c.emoji = QString::fromUtf8(rule.emoji);
        c.color = QString::fromUtf8(rule.color);
        catIndex.insert(c.tag, m_cats.size());
        m_cats.append(std::move(c));
    }

    for (int i = 0; i < m_entries.size(); ++i) {
        const auto& e = m_entries[i];
        if (!entryMatches(e)) continue;
        const auto tag = classifyTag(e.categories);
        const auto it = catIndex.find(tag);
        if (it != catIndex.end()) m_cats[it.value()].entryIdx.append(i);
    }

    // Drop categories with zero matching tools.
    QList<Category> kept;
    kept.reserve(m_cats.size());
    for (auto& c : m_cats) {
        if (!c.entryIdx.isEmpty()) kept.append(std::move(c));
    }
    m_cats = std::move(kept);
}

QModelIndex ToolTreeModel::index(int row, int col, const QModelIndex& parent) const {
    if (col != 0 || row < 0) return {};
    if (!parent.isValid()) {
        if (row >= m_cats.size()) return {};
        return createIndex(row, 0, kCategoryId);
    }
    if (parent.internalId() != kCategoryId) return {}; // leaves have no children
    const int catRow = parent.row();
    if (catRow < 0 || catRow >= m_cats.size()) return {};
    if (row >= m_cats[catRow].entryIdx.size()) return {};
    return createIndex(row, 0, quintptr(catRow + 1));
}

QModelIndex ToolTreeModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) return {};
    const auto id = child.internalId();
    if (id == kCategoryId) return {};
    const int catRow = int(id) - 1;
    return createIndex(catRow, 0, kCategoryId);
}

int ToolTreeModel::rowCount(const QModelIndex& parent) const {
    if (!parent.isValid()) return m_cats.size();
    if (parent.internalId() == kCategoryId) {
        const int r = parent.row();
        if (r < 0 || r >= m_cats.size()) return 0;
        return m_cats[r].entryIdx.size();
    }
    return 0;
}

QVariant ToolTreeModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid()) return {};

    if (idx.internalId() == kCategoryId) {
        if (idx.row() < 0 || idx.row() >= m_cats.size()) return {};
        const auto& c = m_cats[idx.row()];
        switch (role) {
            case Qt::DisplayRole:
            case NameRole:      return c.label;
            case TagRole:       return c.tag;
            case TagColorRole:  return c.color;
            case EmojiRole:     return c.emoji;
            case CountRole:     return c.entryIdx.size();
            case IsLeafRole:    return false;
        }
        return {};
    }

    const int catRow = int(idx.internalId()) - 1;
    if (catRow < 0 || catRow >= m_cats.size()) return {};
    const auto& c = m_cats[catRow];
    if (idx.row() < 0 || idx.row() >= c.entryIdx.size()) return {};
    const int ei = c.entryIdx[idx.row()];
    if (ei < 0 || ei >= m_entries.size()) return {};
    const auto& e = m_entries[ei];

    switch (role) {
        case Qt::DisplayRole:
        case NameRole:        return e.name;
        case GenericRole:     return e.genericName;
        case CommentRole:     return e.comment;
        case ExecRole:        return e.exec;
        // Detect CLI/GUI from BlackArch's own category tag instead of the
        // .desktop Terminal= field.  Every BlackArch tool has Terminal=false
        // because the Exec= line already wraps the binary in a konsole call,
        // so Terminal= is useless for our purposes.  X-BlackArch-CLI /
        // X-BlackArch-GUI is the real signal.
        case IsTerminalRole:  {
            if (e.categories.contains("X-BlackArch-GUI")) return false;
            if (e.categories.contains("X-BlackArch-CLI")) return true;
            return e.terminal;
        }
        case DesktopPathRole: return e.path;
        case TagRole:         return c.tag;
        case TagColorRole:    return c.color;
        case IconRole:        return iconForEntry(e);
        case IsLeafRole:      return true;
        case EntryIndexRole:  return ei;
    }
    return {};
}

QHash<int, QByteArray> ToolTreeModel::roleNames() const {
    return {
        {Qt::DisplayRole, "display"},
        {NameRole, "name"},
        {GenericRole, "generic"},
        {CommentRole, "comment"},
        {IconRole, "iconUrl"},
        {ExecRole, "exec"},
        {TagRole, "tag"},
        {TagColorRole, "tagColor"},
        {IsTerminalRole, "isTerminal"},
        {DesktopPathRole, "desktopPath"},
        {IsLeafRole, "isLeaf"},
        {CountRole, "childCount"},
        {EmojiRole, "emoji"},
        {EntryIndexRole, "entryIndex"},
    };
}

QVariantMap ToolTreeModel::leafAt(const QModelIndex& idx) const {
    QVariantMap m;
    if (!idx.isValid() || idx.internalId() == kCategoryId) return m;
    const auto roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        m.insert(QString::fromUtf8(it.value()), data(idx, it.key()));
    return m;
}

bool ToolTreeModel::isCategory(const QModelIndex& idx) const {
    return idx.isValid() && idx.internalId() == kCategoryId;
}

int ToolTreeModel::entryIndexAt(const QModelIndex& idx) const {
    return data(idx, EntryIndexRole).toInt();
}

void ToolTreeModel::setQueryFilter(const QString& q) {
    if (q == m_query) return;
    beginResetModel();
    m_query = q;
    rebuild();
    endResetModel();
}
