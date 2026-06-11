#pragma once
#include "BusConfigTypes.h"
#include <QObject>
#include <QString>

class BusConfigLoader : public QObject
{
    Q_OBJECT
public:
    explicit BusConfigLoader(QObject *parent = nullptr);

    // Returns a heap-allocated BusConfig (caller takes ownership) or nullptr on error.
    BusConfig *load(const QString &qmlFilePath);

    QString errorString() const { return m_error; }

private:
    QString m_error;
};
