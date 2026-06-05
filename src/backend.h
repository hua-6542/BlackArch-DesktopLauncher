// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QTimer>
#include <QProcess>
#include <QModelIndex>
#include <QAbstractItemModel>
#include <QFileSystemWatcher>

#include "toolmodel.h"

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* toolTree READ toolTree CONSTANT)
    Q_PROPERTY(QStringList backgrounds READ backgrounds NOTIFY backgroundsChanged)
    Q_PROPERTY(QStringList backgroundFolders READ backgroundFolders CONSTANT)
    Q_PROPERTY(bool fallbackActive READ fallbackActive NOTIFY backgroundsChanged)
    Q_PROPERTY(QString selectedToolName READ selectedToolName NOTIFY toolSelected)
    Q_PROPERTY(QString selectedToolGeneric READ selectedToolGeneric NOTIFY toolSelected)
    Q_PROPERTY(QString selectedToolComment READ selectedToolComment NOTIFY toolSelected)
    Q_PROPERTY(QString selectedToolExec READ selectedToolExec NOTIFY toolSelected)
    Q_PROPERTY(int selectedToolEntryIndex READ selectedToolEntryIndex NOTIFY toolSelected)
    Q_PROPERTY(bool selectedToolIsTerminal READ selectedToolIsTerminal NOTIFY toolSelected)
    Q_PROPERTY(QString selectedToolIconUrl READ selectedToolIconUrl NOTIFY toolSelected)
    Q_PROPERTY(QString selectedToolTag READ selectedToolTag NOTIFY toolSelected)
    Q_PROPERTY(QString selectedToolTagColor READ selectedToolTagColor NOTIFY toolSelected)
    Q_PROPERTY(QStringList cornerFrames READ cornerFrames CONSTANT)
    Q_PROPERTY(QString rickAvatar READ rickAvatar CONSTANT)
    Q_PROPERTY(int rollIntervalMs READ rollIntervalMs WRITE setRollIntervalMs NOTIFY configChanged)
    Q_PROPERTY(int rollDurationMs READ rollDurationMs WRITE setRollDurationMs NOTIFY configChanged)
    Q_PROPERTY(QString transitionStyle READ transitionStyle WRITE setTransitionStyle NOTIFY configChanged)
    Q_PROPERTY(bool rollEnabled READ rollEnabled WRITE setRollEnabled NOTIFY configChanged)
    Q_PROPERTY(QString containerStatus READ containerStatus NOTIFY containerStatusChanged)
    Q_PROPERTY(QString containerStatusColor READ containerStatusColor NOTIFY containerStatusChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY toolsChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)

public:
    explicit Backend(QObject* parent = nullptr);

    QAbstractItemModel* toolTree() { return m_tree; }
    QStringList backgrounds() const { return m_backgrounds; }
    QStringList backgroundFolders() const { return m_backgroundFolders; }
    bool fallbackActive() const { return m_fallbackActive; }
    QVariantMap selectedTool() const { return m_selectedTool; }
    QString selectedToolName() const { return m_selectedToolName; }
    QString selectedToolGeneric() const { return m_selectedToolGeneric; }
    QString selectedToolComment() const { return m_selectedToolComment; }
    QString selectedToolExec() const { return m_selectedToolExec; }
    int selectedToolEntryIndex() const { return m_selectedToolEntryIndex; }
    bool selectedToolIsTerminal() const { return m_selectedToolIsTerminal; }
    QString selectedToolIconUrl() const { return m_selectedToolIconUrl; }
    QString selectedToolTag() const { return m_selectedToolTag; }
    QString selectedToolTagColor() const { return m_selectedToolTagColor; }
    QStringList cornerFrames() const { return m_cornerFrames; }
    QString rickAvatar() const { return m_rickAvatar; }

    int rollIntervalMs() const { return m_rollIntervalMs; }
    int rollDurationMs() const { return m_rollDurationMs; }
    QString transitionStyle() const { return m_transitionStyle; }
    bool rollEnabled() const { return m_rollEnabled; }
    QString containerStatus() const { return m_containerStatus; }
    QString containerStatusColor() const { return m_containerStatusColor; }
    int totalCount() const { return m_tree ? m_tree->entries().size() : 0; }
    QString query() const { return m_query; }

    void setRollIntervalMs(int v);
    void setRollDurationMs(int v);
    void setTransitionStyle(const QString& s);
    void setRollEnabled(bool e);
    void setQuery(const QString& q);

public slots:
    // Launch by entry index from the tree leaf (EntryIndexRole).
    void launchEntry(int entryIndex);
    void openDesktopEntry(int entryIndex);
    void recordHistory(const QString& name);
    void refresh();
    Q_INVOKABLE void rescanBackgrounds();
    Q_INVOKABLE void selectTool(int entryIndex);
    void saveConfig();

    // Returns a short usage hint for the given tool name (matched
    // case-insensitively), or a generic fallback.  Static, no I/O.
    Q_INVOKABLE QString usageFor(const QString& name) const;
    Q_INVOKABLE void debugLog(const QString& msg);

signals:
    void toolsChanged();
    void backgroundsChanged();
    void configChanged();
    void containerStatusChanged();
    void queryChanged();
    void error(const QString& message);
    void toolSelected();

private:
    void loadConfig();
    void scanBackgrounds();
    void scanCornerFrames();
    void scanTools();
    void pollContainer();

    ToolTreeModel* m_tree = nullptr;

    QStringList m_backgrounds;
    QStringList m_backgroundFolders;
    bool m_fallbackActive = false;
    QStringList m_cornerFrames;
    QString m_rickAvatar;
    QString m_query;

    int m_rollIntervalMs = 5000;
    int m_rollDurationMs = 1500;
    QString m_transitionStyle = "slide";
    bool m_rollEnabled = true;
    bool m_useCompose = true;
    QString m_terminalCmd = "konsole";
    QString m_containerCmd = "distrobox enter blackarch --";
    QString m_pathBackgrounds;
    QString m_pathBackgroundsCompose;
    QString m_pathIcons;
    QString m_pathGifCorner;

    QString m_containerStatus = "检测中";
    QString m_containerStatusColor = "#888888";

    QTimer m_containerTimer;
    QFileSystemWatcher* m_watcher = nullptr;
    QVariantMap m_selectedTool;
    QString m_selectedToolName;
    QString m_selectedToolGeneric;
    QString m_selectedToolComment;
    QString m_selectedToolExec;
    int m_selectedToolEntryIndex = -1;
    bool m_selectedToolIsTerminal = false;
    QString m_selectedToolIconUrl;
    QString m_selectedToolTag;
    QString m_selectedToolTagColor;
    QStringList m_history;
};
