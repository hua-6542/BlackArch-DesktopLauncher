// SPDX-License-Identifier: MIT
#include "desktopparser.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QSet>

namespace {

QString unescape(QString v) {
    v.replace("\\n", "\n").replace("\\t", "\t").replace("\\\\", "\\");
    return v;
}

} // namespace

DesktopEntry DesktopParser::parseFile(const QString& path) {
    DesktopEntry e;
    e.path = path;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return e;

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);

    bool inEntry = false;
    QString line;
    while (in.readLineInto(&line)) {
        const QString s = line.trimmed();
        if (s.isEmpty() || s.startsWith('#')) continue;
        if (s.startsWith('[') && s.endsWith(']')) {
            inEntry = (s == "[Desktop Entry]");
            continue;
        }
        if (!inEntry) continue;

        const int eq = s.indexOf('=');
        if (eq < 0) continue;
        const QString key = s.left(eq).trimmed();
        const QString val = unescape(s.mid(eq + 1).trimmed());

        // Locale-suffixed keys like Name[zh_CN] — accept zh_CN if present, else fall through.
        // We keep it simple: a later locale-suffixed key wins over the bare one when the
        // suffix matches QLocale::system().name() prefix.
        if (key == "Name")              e.name = val;
        else if (key == "GenericName")  e.genericName = val;
        else if (key == "Comment")      e.comment = val;
        else if (key == "Exec")         e.exec = val;
        else if (key == "Icon")         e.icon = val;
        else if (key == "Terminal")     e.terminal = (val.compare("true", Qt::CaseInsensitive) == 0);
        else if (key == "NoDisplay")    e.noDisplay = (val.compare("true", Qt::CaseInsensitive) == 0);
        else if (key == "Categories")   e.categories = val.split(';', Qt::SkipEmptyParts);
        else if (key == "Keywords")     e.keywords = val.split(';', Qt::SkipEmptyParts);
    }
    return e;
}

QList<DesktopEntry> DesktopParser::parseDirectories(const QStringList& dirs) {
    QHash<QString, DesktopEntry> byName;
    for (const auto& d : dirs) {
        QDir dir(d);
        if (!dir.exists()) continue;
        const auto files = dir.entryInfoList({"*.desktop"}, QDir::Files, QDir::Name);
        for (const auto& fi : files) {
            auto e = parseFile(fi.absoluteFilePath());
            if (e.name.isEmpty() || e.noDisplay || e.exec.isEmpty()) continue;
            byName.insert(e.name, e); // later directories override earlier
        }
    }
    auto list = byName.values();
    std::sort(list.begin(), list.end(), [](const DesktopEntry& a, const DesktopEntry& b){
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return list;
}

QString DesktopParser::cleanExec(QString exec) {
    static const QStringList codes = {"%f","%F","%u","%U","%i","%c","%k"};
    for (const auto& c : codes) exec.remove(c);
    return exec.simplified();
}
