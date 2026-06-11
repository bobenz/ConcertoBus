#include "BusCore.h"
#include "StdioTransport.h"
#include <QJsonDocument>
#include <QJsonArray>

BusCore::BusCore(QObject *parent)
    : QObject(parent)
    , m_router(new Router(this))
    , m_stdio(new StdioTransport(this))
{
    addTransport(m_stdio, false);
    connect(m_router, &Router::sendToClient, this, [this](ClientId id, const QByteArray &json) {
        auto *transport = m_clientTransport.value(id, nullptr);
        if (transport) transport->send(id, json);
    });
}

void BusCore::addTransport(IBusTransport *transport, bool reparent)
{
    if (reparent) transport->setParent(this);

    connect(transport, &IBusTransport::clientConnected,
            this, &BusCore::onClientConnected);
    connect(transport, &IBusTransport::clientDisconnected,
            this, &BusCore::onClientDisconnected);
    connect(transport, &IBusTransport::messageReceived,
            this, &BusCore::onMessageReceived);
}

StdioTransport *BusCore::stdioTransport() const { return m_stdio; }

void BusCore::attachProcess(QProcess *proc, const QStringList &autoSubscribeTags)
{
    // We don't know the ClientId yet — it comes from addProcess → clientConnected.
    // Stash the tags keyed by process pointer; onClientConnected will look them up.
    // Use a temporary connection to capture the id for this specific process.
    connect(m_stdio, &IBusTransport::clientConnected, this,
            [this, proc, autoSubscribeTags](ClientId id) {
                // Check that this id actually belongs to proc.
                // Since addProcess fires clientConnected synchronously we can match
                // by checking m_stdio immediately.
                if (m_pendingAutoSubs.contains(id))
                    return; // already handled
                m_pendingAutoSubs.insert(id, autoSubscribeTags);
                Q_UNUSED(proc)
            }, Qt::SingleShotConnection);

    m_stdio->addProcess(proc);
}

void BusCore::onClientConnected(ClientId id)
{
    auto *transport = qobject_cast<IBusTransport *>(sender());
    if (transport) m_clientTransport.insert(id, transport);
}

void BusCore::onClientDisconnected(ClientId id)
{
    const QString name = m_router->nameOf(id);
    m_router->handleClientGone(id);
    m_clientTransport.remove(id);
    m_pendingAutoSubs.remove(id);
    if (!name.isEmpty()) emit clientGone(id, name);
}

void BusCore::onMessageReceived(ClientId id, const QByteArray &json)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("bad_json")}});
        return;
    }
    dispatchCommand(id, doc.object());
}

void BusCore::dispatchCommand(ClientId id, const QJsonObject &cmd)
{
    const QString c = cmd[QStringLiteral("cmd")].toString();

    if (c == QLatin1String("register")) {
        const QString name = cmd[QStringLiteral("name")].toString();
        if (name.isEmpty()) {
            sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("missing_name")}});
            return;
        }
        if (!m_router->handleRegister(id, name)) {
            sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("name_taken")}});
            return;
        }
        sendJson(id, QJsonObject{{QStringLiteral("ok"), true}});
        // Apply auto-subscribe tags queued by attachProcess
        const QStringList tags = m_pendingAutoSubs.take(id);
        for (const QString &tag : tags)
            m_router->handleSubscribe(id, tag);
        emit clientRegistered(id, name);
        return;
    }

    // All other commands require prior registration
    if (m_router->nameOf(id).isEmpty()) {
        sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("not_registered")}});
        return;
    }

    const QString sender = m_router->nameOf(id);

    if (c == QLatin1String("subscribe")) {
        const QString tag = cmd[QStringLiteral("tag")].toString();
        if (!tag.isEmpty()) m_router->handleSubscribe(id, tag);
        sendJson(id, QJsonObject{{QStringLiteral("ok"), true}});
    } else if (c == QLatin1String("unsubscribe")) {
        const QString tag = cmd[QStringLiteral("tag")].toString();
        if (!tag.isEmpty()) m_router->handleUnsubscribe(id, tag);
        sendJson(id, QJsonObject{{QStringLiteral("ok"), true}});
    } else if (c == QLatin1String("publish")) {
        const QString to   = cmd[QStringLiteral("to")].toString();
        const QJsonObject data = cmd[QStringLiteral("data")].toObject();
        if (!to.isEmpty()) m_router->handlePublish(to, sender, data);
    } else {
        sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("unknown_cmd")}});
    }
}

void BusCore::sendJson(ClientId id, const QJsonObject &obj)
{
    auto *transport = m_clientTransport.value(id, nullptr);
    if (!transport) return;
    QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    transport->send(id, line);
}
