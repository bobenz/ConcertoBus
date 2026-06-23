#pragma once
#include <QObject>
#include <QJsonObject>
#include <QVariantMap>
#include <QtPlugin>

#define IBusGateway_IID "com.concertobus.IBusGateway/1.0"

class IBusGateway : public QObject
{
    Q_OBJECT
public:
    explicit IBusGateway(QObject *parent = nullptr) : QObject(parent) {}

    // Called once after the plugin is loaded.
    virtual bool start(const QVariantMap &config) = 0;

    // Hooks called by BusCore on local bus events.
    virtual void onLocalRegister(const QString &name) = 0;
    virtual void onLocalUnregister(const QString &name) = 0;
    virtual void onLocalSubscribe(const QString &tag) = 0;
    virtual void onLocalUnsubscribe(const QString &tag) = 0;
    virtual void onLocalPublish(const QString &tag, const QString &sender,
                                const QJsonObject &data) = 0;

signals:
    // BusCore connects these to inject remote events into the local bus.
    void remotePublished(const QString &tag, const QString &sender, const QJsonObject &data);
    void remoteEvent(const QString &event, const QString &processName);
    void remoteCommand(const QString &cmd, const QString &processName);
};

Q_DECLARE_INTERFACE(IBusGateway, IBusGateway_IID)
