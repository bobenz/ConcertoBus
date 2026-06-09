#include "BusClientQml.h"

#include <QJsonArray>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QDebug>

// ── BusForeign ────────────────────────────────────────────────────────────────

BusForeign::BusForeign(QObject *parent)
    : QObject(parent)
    , m_client(new BusClient(this))
{
    connect(m_client, &BusClient::messageReceived, this,
            [this](const QString &, const QJsonObject &data) {
                updateFromNetwork(data);
            });
}

QString BusForeign::busHost() const { return m_client->host(); }
void BusForeign::setBusHost(const QString &h) { m_client->setHost(h); emit busHostChanged(); reconnect(); }
int BusForeign::busPort() const { return m_client->port(); }
void BusForeign::setBusPort(int p) { m_client->setPort(p); emit busPortChanged(); reconnect(); }

void BusForeign::setName(const QString &n)
{
    if (m_name == n) return;
    m_name = n;
    emit nameChanged();
    m_client->setName(n);
    reconnect();
}

void BusForeign::setTag(const QString &t)
{
    if (m_tag == t) return;
    if (!m_tag.isEmpty() && m_client->isConnected())
        m_client->unsubscribe(topic());
    m_tag = t;
    emit tagChanged();
    reconnect();
}

void BusForeign::reconnect()
{
    if (m_name.isEmpty() || m_tag.isEmpty()) return;
    m_client->connectToBus();
    connect(m_client, &BusClient::connectedChanged, this, [this]() {
        if (m_client->isConnected())
            m_client->subscribe(topic());
    }, Qt::UniqueConnection);
    if (m_client->isConnected())
        m_client->subscribe(topic());
}

QString BusForeign::topic() const
{
    return m_name + QLatin1Char('/') + m_tag;
}

void BusForeign::updateFromNetwork(const QJsonObject &data)
{
    // Apply property updates
    if (data.contains("properties")) {
        const QJsonObject props = data["properties"].toObject();
        const QMetaObject *mo = metaObject();
        for (auto it = props.begin(); it != props.end(); ++it) {
            const int idx = mo->indexOfProperty(it.key().toUtf8().constData());
            if (idx < 0) continue;
            QMetaProperty prop = mo->property(idx);
            QVariant v = it.value().toVariant();
            if (v.canConvert(prop.metaType()))
                prop.write(this, v);
        }
    }
    // Fire signal by action name
    if (data.contains("action")) {
        const QByteArray sig = data["action"].toString().toUtf8();
        const QMetaObject *mo = metaObject();
        const int idx = mo->indexOfMethod((sig + "()").constData());
        if (idx >= 0)
            mo->method(idx).invoke(this, Qt::QueuedConnection);
    }
}

// ── BusProxy ──────────────────────────────────────────────────────────────────

BusProxy::BusProxy(QObject *parent)
    : QObject(parent)
    , m_client(new BusClient(this))
{}

QString BusProxy::busHost() const { return m_client->host(); }
void BusProxy::setBusHost(const QString &h) { m_client->setHost(h); emit busHostChanged(); reconnect(); }
int BusProxy::busPort() const { return m_client->port(); }
void BusProxy::setBusPort(int p) { m_client->setPort(p); emit busPortChanged(); reconnect(); }

void BusProxy::setTargetProcess(const QString &t)
{
    if (m_target == t) return;
    m_target = t;
    emit targetProcessChanged();
    reconnect();
}

void BusProxy::setTag(const QString &t)
{
    if (m_tag == t) return;
    m_tag = t;
    emit tagChanged();
    reconnect();
}

void BusProxy::reconnect()
{
    if (m_target.isEmpty() || m_tag.isEmpty()) return;
    m_client->connectToBus();
}

QString BusProxy::topic() const
{
    return m_target + QLatin1Char('/') + m_tag;
}

void BusProxy::sendProperties(const QJsonObject &props)
{
    m_client->publish(topic(), QJsonObject{{"properties", props}});
}

void BusProxy::sendAction(const QString &name, const QJsonArray &args)
{
    QJsonObject data{{"action", name}};
    if (!args.isEmpty())
        data["args"] = args;
    m_client->publish(topic(), data);
}
