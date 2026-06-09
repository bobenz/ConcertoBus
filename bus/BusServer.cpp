#include "BusServer.h"
#include "Catalog.h"
#include "Router.h"
#ifdef CONCERTO_BUS_XMPP
#include "XmppGateway.h"
#endif

#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>

BusServer::BusServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_router(new Router(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &BusServer::onNewConnection);

    // When a node registers, notify any launch waiters
    connect(m_router, &Router::launchRequested, this, [](const QString &) {});
}

bool BusServer::listen(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "BusServer: cannot listen on port" << port << m_server->errorString();
        return false;
    }
    qInfo() << "BusServer: listening on port" << m_server->serverPort();
    return true;
}

quint16 BusServer::port() const
{
    return m_server->serverPort();
}

void BusServer::setCatalog(Catalog *catalog)
{
    m_catalog = catalog;
}

#ifdef CONCERTO_BUS_XMPP
void BusServer::setXmppGateway(XmppGateway *gw)
{
    m_xmpp = gw;
}
#endif

void BusServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() { onDisconnected(socket); });
        qDebug() << "BusServer: client connected" << socket->peerAddress().toString();
    }
}

void BusServer::onReadyRead(QTcpSocket *socket)
{
    m_buffers[socket] += socket->readAll();
    QByteArray &buf = m_buffers[socket];
    int nl;
    while ((nl = buf.indexOf('\n')) != -1) {
        const QByteArray line = buf.left(nl).trimmed();
        buf.remove(0, nl + 1);
        if (!line.isEmpty())
            processLine(socket, line);
    }
}

void BusServer::onDisconnected(QTcpSocket *socket)
{
    qDebug() << "BusServer: client disconnected";
    m_router->handleClientGone(socket);
    m_buffers.remove(socket);
    // Remove from any launch waiter lists
    for (auto &waiters : m_launchWaiters)
        waiters.removeAll(socket);
    socket->deleteLater();
}

void BusServer::processLine(QTcpSocket *socket, const QByteArray &line)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        send(socket, QJsonObject{{"error", "invalid_json"}});
        return;
    }
    const QJsonObject msg = doc.object();
    const QString cmd = msg["cmd"].toString();

    if (cmd == "register") {
        const QString name = msg["name"].toString();
        if (name.isEmpty()) { send(socket, QJsonObject{{"error","missing_name"}}); return; }
        m_router->handleRegister(socket, name);
#ifdef CONCERTO_BUS_XMPP
        if (m_xmpp) m_xmpp->onLocalRegister(name);
#endif
        // Notify launch waiters
        if (m_launchWaiters.contains(name)) {
            QJsonObject ready{{"status","ready"},{"name",name}};
            for (QTcpSocket *waiter : m_launchWaiters.take(name))
                send(waiter, ready);
            m_launching.remove(name);
        }
    }
    else if (cmd == "unregister") {
        const QString uname = msg["name"].toString();
        m_router->handleUnregister(uname);
#ifdef CONCERTO_BUS_XMPP
        if (m_xmpp) m_xmpp->onLocalUnregister(uname);
#endif
        send(socket, QJsonObject{{"ok",true}});
    }
    else if (cmd == "subscribe") {
        const QString topic = msg["to"].toString();
        if (topic.isEmpty()) { send(socket, QJsonObject{{"error","missing_to"}}); return; }
        m_router->handleSubscribe(socket, topic);
    }
    else if (cmd == "unsubscribe") {
        m_router->handleUnsubscribe(socket, msg["to"].toString());
        send(socket, QJsonObject{{"ok",true}});
    }
    else if (cmd == "publish") {
        const QString topic = msg["to"].toString();
        const QJsonObject data = msg["data"].toObject();
        if (topic.isEmpty()) { send(socket, QJsonObject{{"error","missing_to"}}); return; }
        m_router->handlePublish(topic, data);
    }
    else if (cmd == "launch") {
        handleLaunch(socket, msg["name"].toString());
    }
    else {
        send(socket, QJsonObject{{"error","unknown_cmd"}});
    }
}

void BusServer::handleLaunch(QTcpSocket *requester, const QString &name)
{
    if (name.isEmpty()) { send(requester, QJsonObject{{"error","missing_name"}}); return; }

    // Already running?
    if (m_router->isRegistered(name)) {
        send(requester, QJsonObject{{"status","ready"},{"name",name}});
        return;
    }

    // Already launching — just add to waiters
    if (m_launching.contains(name)) {
        m_launchWaiters[name].append(requester);
        send(requester, QJsonObject{{"status","launching"}});
        return;
    }

    if (!m_catalog || !m_catalog->contains(name)) {
        send(requester, QJsonObject{{"error","not_found"}});
        return;
    }

    m_launching.insert(name);
    m_launchWaiters[name].append(requester);

    if (m_catalog->launch(name)) {
        send(requester, QJsonObject{{"status","launching"}});
    } else {
        m_launching.remove(name);
        m_launchWaiters.remove(name);
        send(requester, QJsonObject{{"error","launch_failed"}});
    }
}

void BusServer::send(QTcpSocket *socket, const QJsonObject &msg)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        return;
    QByteArray line = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    line += '\n';
    socket->write(line);
}
