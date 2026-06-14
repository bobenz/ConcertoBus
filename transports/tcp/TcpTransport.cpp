#include "TcpTransport.h"
#include <QTcpServer>
#include <QTcpSocket>

TcpTransport::TcpTransport(QObject *parent)
    : IBusTransport(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &TcpTransport::onNewConnection);
}

bool TcpTransport::listen(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qWarning("TcpTransport: cannot listen on port %u: %s",
                 port, qPrintable(m_server->errorString()));
        return false;
    }
    qInfo("TcpTransport: listening on port %u", m_server->serverPort());
    return true;
}

quint16 TcpTransport::port() const
{
    return m_server->serverPort();
}

bool TcpTransport::start(const QVariantMap &config)
{
    return listen(static_cast<quint16>(config.value(QStringLiteral("port"), 49152).toInt()));
}

void TcpTransport::send(ClientId id, const QByteArray &json)
{
    if (QTcpSocket *sock = socketFor(id))
        sock->write(json + '\n');
}

void TcpTransport::closeClient(ClientId id)
{
    if (QTcpSocket *sock = socketFor(id))
        sock->disconnectFromHost();
}

void TcpTransport::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *sock = m_server->nextPendingConnection();
        const ClientId id = static_cast<ClientId>(quintptr(sock));
        m_sockets.insert(id, sock);

        connect(sock, &QTcpSocket::readyRead,    this, &TcpTransport::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &TcpTransport::onDisconnected);

        emit clientConnected(id);
    }
}

void TcpTransport::onReadyRead()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock) return;

    const ClientId id = static_cast<ClientId>(quintptr(sock));
    m_buffers[sock] += sock->readAll();

    int nl;
    while ((nl = m_buffers[sock].indexOf('\n')) != -1) {
        const QByteArray line = m_buffers[sock].left(nl).trimmed();
        m_buffers[sock].remove(0, nl + 1);
        if (!line.isEmpty())
            emit messageReceived(id, line);
    }
}

void TcpTransport::onDisconnected()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock) return;

    const ClientId id = static_cast<ClientId>(quintptr(sock));
    m_sockets.remove(id);
    m_buffers.remove(sock);
    sock->deleteLater();

    emit clientDisconnected(id);
}

QTcpSocket *TcpTransport::socketFor(ClientId id) const
{
    return m_sockets.value(id, nullptr);
}
