#include "XmppGateway.h"

#include <QXmppClient.h>
#include <QXmppConfiguration.h>
#include <QXmppMessage.h>
#include <QXmppPresence.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

XmppGateway::XmppGateway(QObject *parent)
    : IBusGateway(parent)
    , m_client(new QXmppClient(this))
{
    connect(m_client, &QXmppClient::presenceReceived,
            this, &XmppGateway::onPresenceReceived);
    connect(m_client, &QXmppClient::messageReceived,
            this, &XmppGateway::onMessageReceived);
    connect(m_client, &QXmppClient::connected, this, [] {
        qInfo("XmppGateway: connected");
    });
    connect(m_client, &QXmppClient::disconnected, this, [] {
        qWarning("XmppGateway: disconnected");
    });
}

bool XmppGateway::start(const QVariantMap &config)
{
    const QString jid      = config.value(QStringLiteral("user")).toString();
    const QString password = config.value(QStringLiteral("password")).toString();
    const QString server   = config.value(QStringLiteral("server")).toString();
    const int     port     = config.value(QStringLiteral("port"), 5222).toInt();

    // peers is a QStringList stored as QVariant
    const QVariant peersVar = config.value(QStringLiteral("peers"));
    m_peers = peersVar.toStringList();

    if (jid.isEmpty() || password.isEmpty()) {
        qCritical("XmppGateway: 'user' and 'password' are required");
        return false;
    }

    m_bareJid = jid.section(QLatin1Char('/'), 0, 0);

    QXmppConfiguration cfg;
    cfg.setJid(jid);
    cfg.setPassword(password);
    if (!server.isEmpty()) cfg.setHost(server);
    if (port != 5222)      cfg.setPort(port);

    m_client->connectToServer(cfg);
    qInfo() << "XmppGateway: connecting as" << m_bareJid
            << "| peers:" << m_peers;
    return true;
}

// ── Local bus hooks ────────────────────────────────────────────────────────

void XmppGateway::onLocalRegister(const QString &name)
{
    // Advertise the process as an XMPP presence resource so peers can see it.
    QXmppPresence p;
    p.setType(QXmppPresence::Available);
    p.setFrom(m_bareJid + QLatin1Char('/') + name);
    m_client->sendPacket(p);
}

void XmppGateway::onLocalUnregister(const QString &name)
{
    QXmppPresence p;
    p.setType(QXmppPresence::Unavailable);
    p.setFrom(m_bareJid + QLatin1Char('/') + name);
    m_client->sendPacket(p);
}

void XmppGateway::onLocalSubscribe(const QString &tag)
{
    // Tell every peer: "please relay this tag to us".
    QJsonObject body;
    body[QStringLiteral("bus_subscribe")] = true;
    body[QStringLiteral("tag")]           = tag;
    body[QStringLiteral("replyTo")]       = m_bareJid;
    sendToPeers(body);
}

void XmppGateway::onLocalUnsubscribe(const QString &tag)
{
    QJsonObject body;
    body[QStringLiteral("bus_unsubscribe")] = true;
    body[QStringLiteral("tag")]             = tag;
    body[QStringLiteral("replyTo")]         = m_bareJid;
    sendToPeers(body);
}

void XmppGateway::onLocalPublish(const QString &tag, const QString &sender,
                                  const QJsonObject &data)
{
    const QStringList &targets = m_relayTo.value(tag);
    if (targets.isEmpty()) return;

    QJsonObject body;
    body[QStringLiteral("bus_relay")] = true;
    body[QStringLiteral("tag")]       = tag;
    body[QStringLiteral("sender")]    = sender;
    body[QStringLiteral("data")]      = data;

    for (const QString &peer : targets)
        sendToPeer(peer, body);
}

// ── Incoming XMPP events ──────────────────────────────────────────────────

void XmppGateway::onPresenceReceived(const QXmppPresence &presence)
{
    // Resource = process name; bare JID = peer runner.
    const QString fullJid  = presence.from();
    const QString resource = fullJid.section(QLatin1Char('/'), 1);
    const QString peerJid  = fullJid.section(QLatin1Char('/'), 0, 0);

    if (resource.isEmpty() || peerJid == m_bareJid) return;
    if (!m_peers.contains(peerJid)) return; // ignore unknown senders

    if (presence.type() == QXmppPresence::Available) {
        if (!m_remoteProcs[peerJid].contains(resource))
            m_remoteProcs[peerJid].append(resource);
        emit remoteEvent(QStringLiteral("process_started"), resource);
    } else if (presence.type() == QXmppPresence::Unavailable) {
        m_remoteProcs[peerJid].removeAll(resource);
        emit remoteEvent(QStringLiteral("process_stopped"), resource);
    }
}

void XmppGateway::onMessageReceived(const QXmppMessage &msg)
{
    const QString senderJid = msg.from().section(QLatin1Char('/'), 0, 0);
    if (!m_peers.contains(senderJid)) return; // ignore unknown senders

    const QByteArray raw = msg.body().toUtf8();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    const QJsonObject obj = doc.object();

    if (obj.value(QStringLiteral("bus_relay")).toBool()) {
        const QString tag    = obj[QStringLiteral("tag")].toString();
        const QString sender = obj[QStringLiteral("sender")].toString();
        const QJsonObject data = obj[QStringLiteral("data")].toObject();
        if (!tag.isEmpty())
            emit remotePublished(tag, sender, data);
        return;
    }

    if (obj.value(QStringLiteral("bus_subscribe")).toBool()) {
        const QString tag     = obj[QStringLiteral("tag")].toString();
        const QString replyTo = obj[QStringLiteral("replyTo")].toString();
        if (!tag.isEmpty() && !replyTo.isEmpty()
                && !m_relayTo[tag].contains(replyTo))
            m_relayTo[tag].append(replyTo);
        return;
    }

    if (obj.value(QStringLiteral("bus_unsubscribe")).toBool()) {
        const QString tag     = obj[QStringLiteral("tag")].toString();
        const QString replyTo = obj[QStringLiteral("replyTo")].toString();
        if (!tag.isEmpty()) m_relayTo[tag].removeAll(replyTo);
        return;
    }

    if (obj.contains(QStringLiteral("bus_cmd"))) {
        const QString cmd  = obj[QStringLiteral("bus_cmd")].toString();
        const QString name = obj[QStringLiteral("name")].toString();
        if (!cmd.isEmpty() && !name.isEmpty())
            emit remoteCommand(cmd, name);
        return;
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────

void XmppGateway::sendToPeers(const QJsonObject &body)
{
    for (const QString &peer : m_peers)
        sendToPeer(peer, body);
}

void XmppGateway::sendToPeer(const QString &peerJid, const QJsonObject &body)
{
    QXmppMessage msg;
    msg.setTo(peerJid);
    msg.setBody(QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact)));
    m_client->sendPacket(msg);
}
