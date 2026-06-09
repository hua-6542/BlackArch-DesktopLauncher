// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>

struct DesktopEntry {
    QString name;
    QString genericName;
    QString comment;
    QString exec;
    QString icon;
    QStringList categories;
    QStringList keywords;
    bool terminal = false;
    bool noDisplay = false;
    QString path;
};

class DesktopParser {
public:
    static QList<DesktopEntry> parseDirectories(const QStringList& dirs);
    static DesktopEntry parseFile(const QString& path);
    // Strip %f/%u/%U/%F/%i/%c/%k field codes per the Desktop Entry Spec.
    static QString cleanExec(QString exec);
    // Write a .desktop file from a DesktopEntry struct.
    static bool writeDesktopFile(const QString& path, const DesktopEntry& entry);
    // Update only the Icon= field in an existing .desktop file.
    static bool updateDesktopIcon(const QString& path, const QString& newIconPath);
};
