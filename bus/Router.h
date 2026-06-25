#pragma once
#include "IBusTransport.h"
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>

class Router : public QObject
{
    Q_OBJECT
public:
    explicit Router(QObject *parent = nullptr);

    // Called by BusCore when a client sends its first "register" message.
    // Returns false if the name is already taken by a different client.
    bool handleRegister(ClientId id, const QString &name);

    void handleUnsubscribe(ClientId id, const QString &tag);
    void handlePublish(const QString &to, const QString &senderName,
                       const QJsonObject &data);
    void handleClientGone(ClientId id);

    // Subscribe id (already registered as name) to a tag.
    // Drains any queued messages targeted at this client.
    void handleSubscribe(ClientId id, const QString &tag);

    bool isRegistered(const QString &name) const;
    QString nameOf(ClientId id) const;
    ClientId idFor(const QString &name) const;

signals:
    // Router asks BusCore to deliver a serialised push message to one client.
    void sendToClient(ClientId id, const QByteArray &json);

    // Forwarded to gateways after a local publish.
    void published(const QString &tag, const QString &sender,
                   const QJsonObject &data);

private:
    void drainQueuesFor(ClientId id, const QString &name);
    QByteArray makePush(const QString &tag, const QString &sender,
                        const QJsonObject &data) const;

    // name → ClientId
    QHash<QString, ClientId>       m_registry;
    // ClientId → name  (reverse)
    QHash<ClientId, QString>       m_clientName;
    // tag → subscriber ClientIds
    QHash<QString, QSet<ClientId>> m_tagSubs;
    // ClientId → subscribed tags  (reverse)
    QHash<ClientId, QSet<QString>> m_clientTags;
    // queue key → messages
    //   key is "tag:name"  (unicast queue) or "tag:*" (broadcast queue)
    QHash<QString, QList<QJsonObject>> m_queues;
};
