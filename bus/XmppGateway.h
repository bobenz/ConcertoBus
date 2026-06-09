#pragma once
#ifdef CONCERTO_BUS_XMPP

#include <QHash>
#include <QObject>
#include <QStringList>

class QXmppClient;
class QXmppMessage;
class Router;

// XmppGateway connects to an XMPP server as a ConcertoBus federation peer.
//
// Local topology:
//   - On register(name): publish presence resource bus@domain/name
//   - On unregister(name): retract resource presence
//
// Remote subscription flow:
//   1. Local client subscribes to "RemoteNode/tag"
//   2. XmppGateway sends an IQ to the remote Bus JID asking it to relay that topic
//   3. Remote Bus forwards matching publishes as <message> stanzas to our JID
//   4. XmppGateway feeds the payload back into the local Router as a publish
//
// Wire format inside <message> stanzas:
//   <body>{"bus_relay":true,"from":"RemoteNode/tag","data":{...}}</body>
class XmppGateway : public QObject
{
    Q_OBJECT
public:
    explicit XmppGateway(Router *router, QObject *parent = nullptr);

    void connectToServer(const QString &jid, const QString &password, const QString &host = {}, int port = 5222);

    // Called by BusServer when a local node registers/unregisters
    void onLocalRegister(const QString &name);
    void onLocalUnregister(const QString &name);

    // Called by BusServer when a local client subscribes to a topic that looks
    // like it belongs to a remote node (i.e. not locally registered).
    // remoteJid is the bare JID of the remote Bus (e.g. "bus@remote.example.com").
    void requestRemoteSubscription(const QString &topic, const QString &remoteJid);

signals:
    void connected();
    void disconnected();

private slots:
    void onMessageReceived(const QXmppMessage &msg);

private:
    void sendRelayRequest(const QString &topic, const QString &remoteJid);

    Router *m_router;
    QXmppClient *m_client;
    QString m_bareJid;
    // topic → list of remote bus JIDs that want relay pushes
    QHash<QString, QStringList> m_remoteSubscriptions;
};

#endif // CONCERTO_BUS_XMPP
