#pragma once
#include <QAbstractSocket>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

class QTcpSocket;

class BusClient : public QObject
{
    QML_ELEMENT
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)

public:
    explicit BusClient(QObject *parent = nullptr);

    QString host() const { return m_host; }
    void setHost(const QString &h);

    int port() const { return m_port; }
    void setPort(int p);

    QString name() const { return m_name; }
    void setName(const QString &n);

    bool isConnected() const;

    Q_INVOKABLE void connectToBus();
    Q_INVOKABLE void subscribe(const QString &topic);
    Q_INVOKABLE void unsubscribe(const QString &topic);
    Q_INVOKABLE void publish(const QString &topic, const QJsonObject &data);

    // Request that the bus launch a named process.
    // Emits launchReady(name) when it registers, or launchError(name) on failure.
    Q_INVOKABLE void launch(const QString &name);

signals:
    void hostChanged();
    void portChanged();
    void nameChanged();
    void connectedChanged();
    void messageReceived(const QString &from, const QJsonObject &data);
    void launchReady(const QString &name);
    void launchError(const QString &name, const QString &reason);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError err);

private:
    void sendCmd(const QJsonObject &cmd);
    void processLine(const QByteArray &line);

    QTcpSocket *m_socket;
    QByteArray m_buffer;
    QString m_host = "127.0.0.1";
    int m_port = 49152;
    QString m_name;
    bool m_registered = false;
};
