#include "Catalog.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QDebug>

Catalog::Catalog(QObject *parent) : QObject(parent) {}

bool Catalog::load(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Catalog: cannot open" << path;
        return false;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "Catalog: JSON parse error:" << err.errorString();
        return false;
    }
    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        CatalogEntry ce;
        ce.exe = entry["exe"].toString();
        ce.workingDir = entry["workingDir"].toString();
        const QJsonArray argsArr = entry["args"].toArray();
        for (const QJsonValue &v : argsArr)
            ce.args << v.toString();
        m_entries.insert(it.key(), ce);
    }
    qInfo() << "Catalog: loaded" << m_entries.size() << "entries from" << path;
    return true;
}

bool Catalog::contains(const QString &name) const
{
    return m_entries.contains(name);
}

bool Catalog::launch(const QString &name)
{
    if (!m_entries.contains(name)) {
        qWarning() << "Catalog: no entry for" << name;
        return false;
    }
    const CatalogEntry &ce = m_entries.value(name);
    const QString wd = ce.workingDir.isEmpty()
                           ? QFileInfo(ce.exe).absolutePath()
                           : ce.workingDir;
    qInfo() << "Catalog: launching" << name << "->" << ce.exe << ce.args;
    return QProcess::startDetached(ce.exe, ce.args, wd);
}
