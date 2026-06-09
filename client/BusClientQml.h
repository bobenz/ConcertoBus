#pragma once
#include "BusClient.h"
#include <QJsonArray>
#include <QQmlEngine>
#include <QQmlParserStatus>

// ── BusForeign ────────────────────────────────────────────────────────────────
// Subscribes to a topic on the bus. Incoming JSON is applied to user-defined
// properties and signals via QMetaObject reflection — same semantics as
// RemotePlugin's Foreign.
class BusForeign : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
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

    // QQmlParserStatus
    void classBegin() override {}
    void componentComplete() override;

signals:
    void busHostChanged();
    void busPortChanged();
    void nameChanged();
    void tagChanged();

protected:
    void updateFromNetwork(const QString &from, const QJsonObject &data);

private:
    void reconnect();
    QString topic() const;

    BusClient *m_client;
    QString m_name;
    QString m_tag;
    bool m_complete = false;
};

// ── BusProxy ──────────────────────────────────────────────────────────────────
// Hooks all user-defined properties and non-Changed signals via QMetaObject
// reflection (starting from propertyOffset/methodOffset to skip Qt built-ins).
// Publishes changes to the bus as {"properties":{...}} and {"action":...}.
class BusProxy : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString busHost READ busHost WRITE setBusHost NOTIFY busHostChanged)
    Q_PROPERTY(int busPort READ busPort WRITE setBusPort NOTIFY busPortChanged)
    Q_PROPERTY(QString targetProcess READ targetProcess WRITE setTargetProcess NOTIFY targetProcessChanged)
    Q_PROPERTY(QString tag READ tag WRITE setTag NOTIFY tagChanged)

public:
    explicit BusProxy(QObject *parent = nullptr);
    ~BusProxy() override;

    QString busHost() const;
    void setBusHost(const QString &h);
    int busPort() const;
    void setBusPort(int p);
    QString targetProcess() const { return m_target; }
    void setTargetProcess(const QString &t);
    QString tag() const { return m_tag; }
    void setTag(const QString &t);

    bool isHooked() const { return m_hooksEstablished; }
    bool isConnected() const;

    // QQmlParserStatus
    void classBegin() override {}
    void componentComplete() override;

signals:
    void busHostChanged();
    void busPortChanged();
    void targetProcessChanged();
    void tagChanged();

private:
    void reconnect();
    // Wires SignalRelay objects once at componentComplete() — connection-independent.
    void hookPropertiesAndSignals();
    // Pushes current property snapshot on every (re)connect.
    void sendInitialValues();
    QString topic() const;
    void sendPropertyUpdate(const QString &name, const QVariant &value);
    void sendActionTrigger(const QString &name, const QVariantList &args);

    BusClient *m_client;
    QString m_target;
    QString m_tag;
    bool m_complete = false;
    bool m_hooksEstablished = false;
};
