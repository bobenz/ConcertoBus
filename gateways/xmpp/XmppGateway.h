#pragma once
#include "IBusGateway.h"
#include <QHash>
#include <QStringList>

class QXmppClient;
class QXmppMessage;
class QXmppPresence;
class QXmppRosterManager;

// XMPP federation gateway — bridges two ConcertoBus runners over the network.
//
// Addressing: JID per runner. Processes are advertised as XMPP presence resources.
//   bus@machine-a.example.com/SensorApp  → SensorApp is alive on machine A
//
// All bus operations between runners use plain <message> stanzas with a JSON body:
//   bus_subscribe  — ask peer to relay a tag to us
//   bus_unsubscribe — cancel relay
//   bus_relay      — forwarded publish from peer
//   bus_cmd        — launch/kill/restart a process on peer
//   bus_event      — lifecycle notification from peer (process_started/stopped/crashed)
class XmppGateway : public IBusGateway
{
    Q_OBJECT
    Q_INTERFACES(IBusGateway)
    Q_PLUGIN_METADATA(IID IBusGateway_IID FILE "XmppGateway.json")

public:
    explicit XmppGateway(QObject *parent = nullptr);

    // IBusGateway interface
    bool start(const QVariantMap &config) override;
    void onLocalRegister(const QString &name) override;
    void onLocalUnregister(const QString &name) override;
    void onLocalSubscribe(const QString &tag) override;
    void onLocalUnsubscribe(const QString &tag) override;
    void onLocalPublish(const QString &tag, const QString &sender,
                        const QJsonObject &data) override;

private slots:
    void onPresenceReceived(const QXmppPresence &presence);
    void onMessageReceived(const QXmppMessage &msg);

private:
    void sendToPeers(const QJsonObject &body);
    void sendToPeer(const QString &peerJid, const QJsonObject &body);

    void flushPendingPackets();

    QXmppClient        *m_client;
    QXmppRosterManager *m_roster = nullptr;
    QString      m_bareJid;
    QStringList  m_peers;                       // peer runner bare JIDs from config
    QHash<QString, QStringList> m_relayTo;         // tag → peer bare JIDs that subscribed for relay
    QHash<QString, QStringList> m_remoteProcs;    // peerJid → registered process names
    QStringList  m_localTags;                     // tags we have locally subscribed
    QStringList  m_onlinePeers;                   // peer bare JIDs currently seen online
    QStringList  m_announcedTo;                   // peers we have sent our subscriptions to
    bool         m_connected = false;
    QList<QJsonObject> m_pendingToSend;         // queued until XMPP connected
};
