#pragma once
#include "IBusTransport.h"
#include <QHash>

class QTcpServer;
class QTcpSocket;

// Built-in TCP transport: listens on a port, assigns ClientId = quintptr(socket).
// Socket pointer addresses are large enough to never collide with StdioTransport's
// small integer IDs.
class TcpTransport : public IBusTransport
{
    Q_OBJECT
public:
    explicit TcpTransport(QObject *parent = nullptr);

    bool listen(quint16 port = 49152);
    quint16 port() const;

    // IBusTransport
    bool start(const QVariantMap &config) override;
    void send(ClientId id, const QByteArray &json) override;
    void closeClient(ClientId id) override;

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket *socketFor(ClientId id) const;

    QTcpServer *m_server;
    QHash<ClientId, QTcpSocket *> m_sockets;  // id → socket
    QHash<QTcpSocket *, QByteArray> m_buffers; // per-socket read buffer
};
