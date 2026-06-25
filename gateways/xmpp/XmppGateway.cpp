#include "XmppGateway.h"

#include <QXmppClient.h>
#include <QXmppConfiguration.h>
#include <QXmppMessage.h>
#include <QXmppPresence.h>
#include <QXmppRosterManager.h>

#include <QXmppLogger.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

XmppGateway::XmppGateway(QObject *parent)
    : IBusGateway(parent)
    , m_client(new QXmppClient(this))
{
    auto *logger = QXmppLogger::getLogger();
    logger->setLoggingType(QXmppLogger::StdoutLogging);
    logger->setMessageTypes(QXmppLogger::AnyMessage);
    m_client->setLogger(logger);

    m_roster = m_client->findExtension<QXmppRosterManager>();

    connect(m_client, &QXmppClient::presenceReceived,
            this, &XmppGateway::onPresenceReceived);
    connect(m_client, &QXmppClient::messageReceived,
            this, &XmppGateway::onMessageReceived);
    connect(m_client, &QXmppClient::connected, this, [this] {
        m_connected = true;
        qInfo("XmppGateway: connected");
        // Boost priority so bare-JID messages route here, not to other clients (e.g. Psi+).
        QXmppPresence pres;
        pres.setPriority(100);
        m_client->setClientPresence(pres);
        // Ensure every peer is in our roster so the server accepts messages both ways.
        if (m_roster) {
            const auto contacts = m_roster->getRosterBareJids();
            for (const QString &peer : m_peers) {
                if (!contacts.contains(peer)) {
                    QXmppPresence sub;
                    sub.setType(QXmppPresence::Subscribe);
                    sub.setTo(peer);
                    m_client->sendPacket(sub);
                }
            }
        }
        flushPendingPackets();
    });
    // Auto-accept subscription requests from our configured peers.
    if (m_roster) {
        connect(m_roster, &QXmppRosterManager::subscriptionReceived, this,
                [this](const QString &bareJid) {
                    if (m_peers.contains(bareJid)) {
                        QXmppPresence subscribed;
                        subscribed.setType(QXmppPresence::Subscribed);
                        subscribed.setTo(bareJid);
                        m_client->sendPacket(subscribed);
                    }
                });
    }
    connect(m_client, &QXmppClient::disconnected, this, [this] {
        m_connected = false;
        m_onlinePeers.clear();
        m_announcedTo.clear();
        // Clear relay table — peers will re-subscribe when connection recovers.
        m_relayTo.clear();
        // Re-queue our own subscriptions so they are re-announced on reconnect.
        for (const QString &tag : m_localTags) {
            QJsonObject body;
            body[QStringLiteral("bus_subscribe")] = true;
            body[QStringLiteral("tag")]           = tag;
            body[QStringLiteral("replyTo")]       = m_bareJid;
            for (const QString &peer : m_peers) {
                QJsonObject tagged = body;
                tagged[QStringLiteral("__to")] = peer;
                m_pendingToSend.append(tagged);
            }
        }
        qWarning("XmppGateway: disconnected");
    });
    connect(m_client, &QXmppClient::error, this, [this](QXmppClient::Error err) {
        qWarning() << "XmppGateway: error" << err
                   << m_client->xmppStreamError();
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

    cfg.setStreamSecurityMode(QXmppConfiguration::TLSDisabled);
    cfg.setSaslAuthMechanism("DIGEST-MD5");

    m_client->connectToServer(cfg);
    qInfo() << "XmppGateway: connecting as" << m_bareJid
            << "| peers:" << m_peers;
    return true;
}

// ── Local bus hooks ────────────────────────────────────────────────────────

void XmppGateway::onLocalRegister(const QString &name)
{
    if (!m_connected) return; // will re-announce on reconnect via flushPendingPackets
    QXmppPresence p;
    p.setType(QXmppPresence::Available);
    p.setFrom(m_bareJid + QLatin1Char('/') + name);
    m_client->sendPacket(p);
}

void XmppGateway::onLocalUnregister(const QString &name)
{
    if (!m_connected) return;
    QXmppPresence p;
    p.setType(QXmppPresence::Unavailable);
    p.setFrom(m_bareJid + QLatin1Char('/') + name);
    m_client->sendPacket(p);
}

void XmppGateway::onLocalSubscribe(const QString &tag)
{
    qInfo() << "XmppGateway: onLocalSubscribe tag=" << tag << "peers=" << m_peers;
    if (!m_localTags.contains(tag))
        m_localTags.append(tag);
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
    qInfo() << "XmppGateway: onLocalPublish tag=" << tag
            << "sender=" << sender << "relayTargets=" << targets;
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

    if (peerJid == m_bareJid) return;
    if (!m_peers.contains(peerJid)) return; // ignore unknown senders

    if (presence.type() == QXmppPresence::Available) {
        // When a new peer runner bare JID comes online, re-announce all our
        // local subscriptions so startup order doesn't matter.
        if (!m_onlinePeers.contains(peerJid)) {
            m_onlinePeers.append(peerJid);
            qInfo() << "XmppGateway: peer online" << peerJid
                    << "- re-announcing" << m_localTags.size() << "local tags";
            for (const QString &tag : m_localTags) {
                QJsonObject body;
                body[QStringLiteral("bus_subscribe")] = true;
                body[QStringLiteral("tag")]           = tag;
                body[QStringLiteral("replyTo")]       = m_bareJid;
                sendToPeer(peerJid, body);
            }
        }
        // Resource beyond the runner-level JID = bus process name (lifecycle event)
        if (!resource.isEmpty()) {
            if (!m_remoteProcs[peerJid].contains(resource))
                m_remoteProcs[peerJid].append(resource);
            emit remoteEvent(QStringLiteral("process_started"), resource);
        }
    } else if (presence.type() == QXmppPresence::Unavailable) {
        m_onlinePeers.removeAll(peerJid);
        if (!resource.isEmpty()) {
            m_remoteProcs[peerJid].removeAll(resource);
            emit remoteEvent(QStringLiteral("process_stopped"), resource);
        }
    }
}

void XmppGateway::onMessageReceived(const QXmppMessage &msg)
{
    const QString senderJid = msg.from().section(QLatin1Char('/'), 0, 0);
    qInfo() << "XmppGateway: onMessageReceived from=" << msg.from()
            << "bareJid=" << senderJid << "knownPeer=" << m_peers.contains(senderJid)
            << "body=" << msg.body().left(120);
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
        // Reciprocate: peer just announced it's alive — send our subscriptions back
        // so it can relay our tags to us. Idempotent: only once per peer per session.
        if (!replyTo.isEmpty() && !m_announcedTo.contains(replyTo)) {
            m_announcedTo.append(replyTo);
            for (const QString &localTag : m_localTags) {
                QJsonObject body;
                body[QStringLiteral("bus_subscribe")] = true;
                body[QStringLiteral("tag")]           = localTag;
                body[QStringLiteral("replyTo")]       = m_bareJid;
                sendToPeer(replyTo, body);
            }
        }
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
    QJsonObject tagged = body;
    tagged[QStringLiteral("__to")] = peerJid;

    if (!m_connected) {
        m_pendingToSend.append(tagged);
        return;
    }

    QXmppMessage msg;
    msg.setTo(peerJid);
    msg.setBody(QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact)));
    m_client->sendPacket(msg);
}

void XmppGateway::flushPendingPackets()
{
    const auto pending = m_pendingToSend;
    m_pendingToSend.clear();
    for (const QJsonObject &tagged : pending) {
        const QString to = tagged[QStringLiteral("__to")].toString();
        QJsonObject body = tagged;
        body.remove(QStringLiteral("__to"));
        QXmppMessage msg;
        msg.setTo(to);
        msg.setBody(QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact)));
        m_client->sendPacket(msg);
    }
}
