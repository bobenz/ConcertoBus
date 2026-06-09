#include "Router.h"
#include <QJsonDocument>
#include <QTcpSocket>

Router::Router(QObject *parent) : QObject(parent) {}

void Router::handleRegister(QTcpSocket *socket, const QString &name)
{
    // Replace any stale entry for the same name
    if (m_registry.contains(name)) {
        QTcpSocket *old = m_registry.value(name);
        if (old != socket)
            m_socketName.remove(old);
    }
    m_registry.insert(name, socket);
    m_socketName.insert(socket, name);

    // Drain all queued messages for topics owned by this node
    const QString prefix = name + QLatin1Char('/');
    for (auto it = m_queues.begin(); it != m_queues.end(); ) {
        if (it.key().startsWith(prefix)) {
            drainQueue(it.key(), socket);
            it = m_queues.erase(it);
        } else {
            ++it;
        }
    }

    send(socket, QJsonObject{{"ok", true}});
}

void Router::handleUnregister(const QString &name)
{
    QTcpSocket *socket = m_registry.take(name);
    if (socket)
        m_socketName.remove(socket);
}

void Router::handleSubscribe(QTcpSocket *socket, const QString &topic)
{
    m_subscriptions[topic].insert(socket);
    m_socketTopics[socket].insert(topic);
    // Drain any queued messages to this new subscriber
    if (m_queues.contains(topic)) {
        for (const QJsonObject &data : m_queues.take(topic)) {
            QJsonObject push;
            push["push"]  = true;
            push["from"]  = topic;
            push["data"]  = data;
            send(socket, push);
        }
    }
    send(socket, QJsonObject{{"ok", true}});
}

void Router::handleUnsubscribe(QTcpSocket *socket, const QString &topic)
{
    m_subscriptions[topic].remove(socket);
    m_socketTopics[socket].remove(topic);
}

void Router::handlePublish(const QString &topic, const QJsonObject &data)
{
    const QSet<QTcpSocket *> &subs = m_subscriptions.value(topic);

    if (subs.isEmpty()) {
        // No subscribers yet — queue for the next subscriber that arrives
        m_queues[topic].append(data);
        return;
    }

    QJsonObject push;
    push["push"] = true;
    push["from"] = topic;
    push["data"] = data;

    for (QTcpSocket *sub : subs)
        send(sub, push);

    emit published(topic, data);
}

void Router::handleClientGone(QTcpSocket *socket)
{
    // Remove registration
    if (m_socketName.contains(socket)) {
        const QString name = m_socketName.take(socket);
        m_registry.remove(name);
    }

    // Remove all subscriptions
    const QSet<QString> topics = m_socketTopics.take(socket);
    for (const QString &topic : topics)
        m_subscriptions[topic].remove(socket);
}

bool Router::isRegistered(const QString &name) const
{
    return m_registry.contains(name);
}

void Router::drainQueue(const QString &topic, QTcpSocket *socket)
{
    const QList<QJsonObject> &msgs = m_queues.value(topic);
    for (const QJsonObject &data : msgs) {
        QJsonObject push;
        push["push"] = true;
        push["from"] = topic;
        push["data"] = data;
        send(socket, push);
    }
}

void Router::send(QTcpSocket *socket, const QJsonObject &msg)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        return;
    QByteArray line = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    line += '\n';
    socket->write(line);
}
