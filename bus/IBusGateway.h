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

    virtual bool start(const QVariantMap &config) = 0;
    virtual void onLocalPublish(const QString &tag, const QString &sender,
                                const QJsonObject &data) = 0;
    virtual void onLocalRegister(const QString &name) = 0;
    virtual void onLocalUnregister(const QString &name) = 0;

signals:
    void remotePublished(const QString &topic, const QJsonObject &data);
};

Q_DECLARE_INTERFACE(IBusGateway, IBusGateway_IID)
