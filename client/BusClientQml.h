#pragma once
#include "BusClient.h"
#include <QJsonArray>
#include <QQmlEngine>

// ── BusForeign ────────────────────────────────────────────────────────────────
// Subscribes to a topic on the bus and exposes incoming messages via
// updateFromNetwork() — same interface as RemotePlugin's Foreign.
class BusForeign : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString busHost READ busHost WRITE setBusHost NOTIFY busHostChanged)
    Q_PROPERTY(int busPort READ busPort WRITE setBusPort NOTIFY busPortChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString tag READ tag WRITE setTag NOTIFY tagChanged)

public:
    explicit BusForeign(QObject *parent = nullptr);

    QString busHost() const;
    void setBusHost(const QString &h);
    int busPort() const;
    void setBusPort(int p);
    QString name() const { return m_name; }
    void setName(const QString &n);
    QString tag() const { return m_tag; }
    void setTag(const QString &t);

signals:
    void busHostChanged();
    void busPortChanged();
    void nameChanged();
    void tagChanged();

protected:
    // Subclasses / QML property bindings apply data here.
    // Dispatches {"properties":{...}} and {"action":...} exactly as Foreign did.
    virtual void updateFromNetwork(const QJsonObject &data);

private:
    void reconnect();
    QString topic() const;

    BusClient *m_client;
    QString m_name;
    QString m_tag;
};

// ── BusProxy ──────────────────────────────────────────────────────────────────
// Publishes property changes and signal triggers to a topic on the bus.
// Mirrors RemotePlugin's Proxy API.
class BusProxy : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString busHost READ busHost WRITE setBusHost NOTIFY busHostChanged)
    Q_PROPERTY(int busPort READ busPort WRITE setBusPort NOTIFY busPortChanged)
    Q_PROPERTY(QString targetProcess READ targetProcess WRITE setTargetProcess NOTIFY targetProcessChanged)
    Q_PROPERTY(QString tag READ tag WRITE setTag NOTIFY tagChanged)

public:
    explicit BusProxy(QObject *parent = nullptr);

    QString busHost() const;
    void setBusHost(const QString &h);
    int busPort() const;
    void setBusPort(int p);
    QString targetProcess() const { return m_target; }
    void setTargetProcess(const QString &t);
    QString tag() const { return m_tag; }
    void setTag(const QString &t);

    // Called by the QML engine's property/signal hooks
    Q_INVOKABLE void sendProperties(const QJsonObject &props);
    Q_INVOKABLE void sendAction(const QString &name, const QJsonArray &args = QJsonArray());

signals:
    void busHostChanged();
    void busPortChanged();
    void targetProcessChanged();
    void tagChanged();

private:
    void reconnect();
    QString topic() const;

    BusClient *m_client;
    QString m_target;
    QString m_tag;
};
