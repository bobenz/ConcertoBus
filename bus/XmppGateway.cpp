#ifdef CONCERTO_BUS_XMPP

#include "XmppGateway.h"
#include "Router.h"

#include <QXmppClient.h>
#include <QXmppConfiguration.h>
#include <QXmppMessage.h>
#include <QXmppPresence.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

XmppGateway::XmppGateway(Router *router, QObject *parent)
    : QObject(parent)
    , m_router(router)
    , m_client(new QXmppClient(this))
{
    connect(m_client, &QXmppClient::connected, this, &XmppGateway::connected);
    connect(m_client, &QXmppClient::disconnected, this, &XmppGateway::disconnected);
    connect(m_client, &QXmppClient::messageReceived, this, &XmppGateway::onMessageReceived);

    // Forward locally published messages to any interested remote buses
    connect(m_router, &Router::published, this, [this](const QString &topic, const QJsonObject &data) {
        const QList<QString> remotes = m_remoteSubscriptions.value(topic);
        if (remotes.isEmpty()) return;
        QJsonObject body;
        body["bus_relay"] = true;
        body["from"] = topic;
        body["data"] = data;
        const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
        for (const QString &jid : remotes) {
            QXmppMessage msg;
            msg.setTo(jid);
            msg.setBody(QString::fromUtf8(payload));
            m_client->sendPacket(msg);
        }
    });
}

void XmppGateway::connectToServer(const QString &jid, const QString &password,
                                   const QString &host, int port)
{
    m_bareJid = jid.section(QLatin1Char('/'), 0, 0);
    QXmppConfiguration cfg;
    cfg.setJid(jid);
    cfg.setPassword(password);
    if (!host.isEmpty()) cfg.setHost(host);
    if (port != 5222) cfg.setPort(port);
    m_client->connectToServer(cfg);
}

void XmppGateway::onLocalRegister(const QString &name)
{
    QXmppPresence p;
    p.setType(QXmppPresence::Available);
    p.setStatusText(name);
    m_client->sendPacket(p);
}

void XmppGateway::onLocalUnregister(const QString &name)
{
    Q_UNUSED(name)
    QXmppPresence p;
    p.setType(QXmppPresence::Unavailable);
    m_client->sendPacket(p);
}

void XmppGateway::requestRemoteSubscription(const QString &topic, const QString &remoteJid)
{
    QJsonObject body;
    body["bus_subscribe"] = true;
    body["topic"] = topic;
    body["replyTo"] = m_bareJid;

    QXmppMessage msg;
    msg.setTo(remoteJid);
    msg.setBody(QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact)));
    m_client->sendPacket(msg);
}

void XmppGateway::onMessageReceived(const QXmppMessage &msg)
{
    const QByteArray raw = msg.body().toUtf8();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    const QJsonObject obj = doc.object();

    if (obj.value("bus_relay").toBool()) {
        // Remote Bus forwarding a publish to us; inject into local router
        const QString from = obj["from"].toString();
        const QJsonObject data = obj["data"].toObject();
        if (!from.isEmpty() && !data.isEmpty())
            m_router->handlePublish(from, data);
        return;
    }

    if (obj.value("bus_subscribe").toBool()) {
        // Remote Bus asking us to relay a topic to them
        const QString topic = obj["topic"].toString();
        const QString replyTo = obj["replyTo"].toString();
        if (topic.isEmpty() || replyTo.isEmpty()) return;
        if (!m_remoteSubscriptions[topic].contains(replyTo))
            m_remoteSubscriptions[topic].append(replyTo);
    }
}

#endif // CONCERTO_BUS_XMPP
