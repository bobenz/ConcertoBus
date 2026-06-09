#pragma once
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

struct CatalogEntry {
    QString exe;
    QStringList args;
    QString workingDir;
};

class Catalog : public QObject
{
    Q_OBJECT
public:
    explicit Catalog(QObject *parent = nullptr);

    bool load(const QString &path);
    bool contains(const QString &name) const;
    bool launch(const QString &name);

private:
    QHash<QString, CatalogEntry> m_entries;
};
