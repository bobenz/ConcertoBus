#include "Router.h"
#include <QJsonDocument>

Router::Router(QObject *parent) : QObject(parent) {}

bool Router::handleRegister(ClientId id, const QString &name)
{
    if (m_registry.contains(name))
        return false;
    m_registry.insert(name, id);
    m_clientName.insert(id, name);
    m_clientTags.insert(id, {});
    return true;
}

void Router::handleSubscribe(ClientId id, const QString &tag)
{
    m_tagSubs[tag].insert(id);
    m_clientTags[id].insert(tag);

    const QString name = m_clientName.value(id);
    // Drain unicast queue ("tag:name") and broadcast queue ("tag:*") for this tag.
    auto drainKey = [&](const QString &key) {
        auto qit = m_queues.find(key);
        if (qit == m_queues.end()) return;
        for (const QJsonObject &msg : qit.value())
            emit sendToClient(id, makePush(tag, {}, msg));
        m_queues.erase(qit);
    };
    drainKey(tag + QLatin1Char(':') + name);
    drainKey(tag + QLatin1String(":*"));
}

void Router::handleUnsubscribe(ClientId id, const QString &tag)
{
    m_tagSubs[tag].remove(id);
    m_clientTags[id].remove(tag);
}

void Router::handlePublish(const QString &to, const QString &senderName,
                           const QJsonObject &data)
{
    QString tag;
    QString target;

    const int colon = to.indexOf(QLatin1Char(':'));
    if (colon >= 0) {
        tag    = to.left(colon);
        target = to.mid(colon + 1);
    } else {
        tag    = to;
        target = QStringLiteral("*");
    }

    emit published(tag, senderName, data);

    const QSet<ClientId> &tagSubscribers = m_tagSubs.value(tag);

    if (target == QLatin1String("*")) {
        if (tagSubscribers.isEmpty()) {
            m_queues[tag + QLatin1String(":*")].append(data);
        } else {
            const QByteArray msg = makePush(tag, senderName, data);
            for (ClientId rid : tagSubscribers)
                emit sendToClient(rid, msg);
        }
    } else {
        // unicast to a specific registered name
        auto it = m_registry.find(target);
        if (it == m_registry.end()) {
            m_queues[tag + QLatin1Char(':') + target].append(data);
            return;
        }
        ClientId rid = it.value();
        if (!tagSubscribers.contains(rid)) {
            m_queues[tag + QLatin1Char(':') + target].append(data);
            return;
        }
        emit sendToClient(rid, makePush(tag, senderName, data));
    }
}

void Router::handleClientGone(ClientId id)
{
    const QString name = m_clientName.take(id);
    m_registry.remove(name);

    const QSet<QString> tags = m_clientTags.take(id);
    for (const QString &t : tags)
        m_tagSubs[t].remove(id);
}

bool Router::isRegistered(const QString &name) const
{
    return m_registry.contains(name);
}

QString Router::nameOf(ClientId id) const
{
    return m_clientName.value(id);
}

QByteArray Router::makePush(const QString &tag, const QString &sender,
                             const QJsonObject &data) const
{
    QJsonObject msg;
    msg[QStringLiteral("push")]   = true;
    msg[QStringLiteral("from")]   = tag;
    msg[QStringLiteral("sender")] = sender;
    msg[QStringLiteral("data")]   = data;
    return QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n';
}
