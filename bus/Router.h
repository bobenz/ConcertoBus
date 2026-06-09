#pragma once
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>

class QTcpSocket;

class Router : public QObject
{
    Q_OBJECT
public:
    explicit Router(QObject *parent = nullptr);

    void handleRegister(QTcpSocket *socket, const QString &name);
    void handleUnregister(const QString &name);
    void handleSubscribe(QTcpSocket *socket, const QString &topic);
    void handleUnsubscribe(QTcpSocket *socket, const QString &topic);
    void handlePublish(const QString &topic, const QJsonObject &data);
    void handleClientGone(QTcpSocket *socket);

    bool isRegistered(const QString &name) const;

signals:
    void launchRequested(const QString &name);

private:
    void drainQueue(const QString &nodePrefix, QTcpSocket *socket);
    static void send(QTcpSocket *socket, const QJsonObject &msg);

    // name → owning socket (the registered process)
    QHash<QString, QTcpSocket *> m_registry;
    // topic ("ProcessA/sensor") → subscriber sockets
    QHash<QString, QSet<QTcpSocket *>> m_subscriptions;
    // topic → queued messages while owner is offline
    QHash<QString, QList<QJsonObject>> m_queues;
    // reverse map: socket → registered name (for cleanup)
    QHash<QTcpSocket *, QString> m_socketName;
    // reverse map: socket → subscribed topics (for cleanup)
    QHash<QTcpSocket *, QSet<QString>> m_socketTopics;
};
