// SPDX-License-Identifier: MIT
#pragma once

#include <QAbstractItemModel>
#include <QStringList>
#include "desktopparser.h"

// Two-level tree:
//   level 0: category nodes (Recon / Web / …) — `data` holds the tag string
//   level 1: tool leaves     — `data` holds the index into the entries list
//
// The model owns the entries; setEntries() resets the tree.
class ToolTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        GenericRole,
        CommentRole,
        IconRole,
        ExecRole,
        TagRole,
        TagColorRole,
        IsTerminalRole,
        DesktopPathRole,
        IsLeafRole,
        CountRole,        // category-only: number of child tools
        EmojiRole,         // category-only
        EntryIndexRole,   // leaf-only: index into m_entries
    };

    explicit ToolTreeModel(QObject* parent = nullptr);

    QModelIndex index(int row, int col, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override { Q_UNUSED(parent); return 1; }
    QVariant data(const QModelIndex& idx, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QList<DesktopEntry> entries);
    const QList<DesktopEntry>& entries() const { return m_entries; }

    // Map a row in the flat ToolModel/proxy back into this tree, or vice
    // versa.  Used by the search filter so typing collapses categories
    // and shows matching tools regardless of group.
    Q_INVOKABLE QVariantMap leafAt(const QModelIndex& idx) const;

    // Returns true if the given index points at a category (level-0) node.
    Q_INVOKABLE bool isCategory(const QModelIndex& idx) const;

    // Returns the entry index (into m_entries) for a leaf node, or -1 for categories.
    Q_INVOKABLE int entryIndexAt(const QModelIndex& idx) const;

    // Apply a search filter to the leaf nodes.  Categories with zero
    // matching children are hidden as well.  An empty query restores all.
    void setQueryFilter(const QString& q);

    QString query() const { return m_query; }

private:
    struct Category {
        QString tag;
        QString label;
        QString emoji;
        QString color;
        QList<int> entryIdx; // indices into m_entries
    };

    void rebuild();
    bool entryMatches(const DesktopEntry& e) const;

    QList<DesktopEntry> m_entries;
    QList<Category> m_cats;     // visible categories (after filter)
    QString m_query;
};
