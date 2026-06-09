#pragma once
#include <QHash>
#include <QObject>
#include <QSet>

class QTcpServer;
class QTcpSocket;
class Router;
class Catalog;
#ifdef CONCERTO_BUS_XMPP
class XmppGateway;
#endif

class BusServer : public QObject
{
    Q_OBJECT
public:
    explicit BusServer(QObject *parent = nullptr);

    bool listen(quint16 port = 49152);
    quint16 port() const;
    void setCatalog(Catalog *catalog);
    Router *router() const { return m_router; }
#ifdef CONCERTO_BUS_XMPP
    void setXmppGateway(XmppGateway *gw);
#endif

private slots:
    void onNewConnection();

private:
    void onReadyRead(QTcpSocket *socket);
    void onDisconnected(QTcpSocket *socket);
    void processLine(QTcpSocket *socket, const QByteArray &line);
    void handleLaunch(QTcpSocket *requester, const QString &name);
    static void send(QTcpSocket *socket, const QJsonObject &msg);

    QTcpServer *m_server;
    Router *m_router;
    Catalog *m_catalog = nullptr;
#ifdef CONCERTO_BUS_XMPP
    XmppGateway *m_xmpp = nullptr;
#endif
    QHash<QTcpSocket *, QByteArray> m_buffers;
    // launch: name → list of sockets waiting for it to register
    QHash<QString, QList<QTcpSocket *>> m_launchWaiters;
    QSet<QString> m_launching;
};
