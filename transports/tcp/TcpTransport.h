#pragma once
#include "IBusTransport.h"
#include <QHash>

class QTcpServer;
class QTcpSocket;

class TcpTransport : public IBusTransport
{
    Q_OBJECT
    Q_INTERFACES(IBusTransport)
    Q_PLUGIN_METADATA(IID IBusTransport_IID FILE "TcpTransport.json")
    Q_PROPERTY(quint16 port READ port CONSTANT)
public:
    explicit TcpTransport(QObject *parent = nullptr);

    bool listen(quint16 port = 49152);
    quint16 port() const;

    // IBusTransport
    IBusTransport *createInstance(QObject *parent = nullptr) override;
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
