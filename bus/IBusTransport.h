#pragma once
#include <QObject>
#include <QByteArray>
#include <QVariantMap>
#include <QtPlugin>

using ClientId = quintptr;

#define IBusTransport_IID "com.concertobus.IBusTransport/1.0"

class IBusTransport : public QObject
{
    Q_OBJECT
public:
    explicit IBusTransport(QObject *parent = nullptr) : QObject(parent) {}

    // Factory: return a fresh, independent transport instance.
    // The plugin root object itself is never used as an active transport.
    virtual IBusTransport *createInstance(QObject *parent = nullptr) = 0;

    virtual bool start(const QVariantMap &config) = 0;
    virtual void send(ClientId id, const QByteArray &json) = 0;
    virtual void closeClient(ClientId id) = 0;

signals:
    void clientConnected(ClientId id);
    void clientDisconnected(ClientId id);
    void messageReceived(ClientId id, const QByteArray &json);
};

Q_DECLARE_INTERFACE(IBusTransport, IBusTransport_IID)
