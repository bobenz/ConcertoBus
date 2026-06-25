#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>

class AbstractBusClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)

public:
    explicit AbstractBusClient(QObject *parent = nullptr) : QObject(parent) {}

    virtual QString name() const = 0;
    virtual void setName(const QString &name) = 0;
    virtual bool isConnected() const = 0;

    Q_INVOKABLE virtual void connectToBus() = 0;
    Q_INVOKABLE virtual void subscribe(const QString &tag) = 0;
    Q_INVOKABLE virtual void unsubscribe(const QString &tag) = 0;
    Q_INVOKABLE virtual void publish(const QString &to, const QJsonObject &data) = 0;
    Q_INVOKABLE virtual void launch(const QString &name) = 0;
    Q_INVOKABLE virtual void injectQml(const QString &target, const QString &name,
                                       const QString &url    = {},
                                       const QString &source = {}) = 0;

signals:
    void nameChanged();
    void connectedChanged();
    void messageReceived(const QString &tag, const QString &sender, const QJsonObject &data);
    void errorOccurred(const QString &error);
    void injectionRequested(const QString &name, const QString &url, const QString &source);
};
