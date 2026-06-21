#pragma once
#include "AbstractBusClient.h"
#include <QAbstractSocket>
#include <QtQml/qqml.h>

class QTcpSocket;

class BusClient : public AbstractBusClient
{
    QML_ELEMENT
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)

public:
    explicit BusClient(QObject *parent = nullptr);

    // AbstractBusClient
    QString name() const override { return m_name; }
    void setName(const QString &n) override;
    bool isConnected() const override;

    QString host() const { return m_host; }
    void setHost(const QString &h);
    int port() const { return m_port; }
    void setPort(int p);

    Q_INVOKABLE void connectToBus() override;
    Q_INVOKABLE void subscribe(const QString &tag) override;
    Q_INVOKABLE void unsubscribe(const QString &tag) override;
    Q_INVOKABLE void publish(const QString &to, const QJsonObject &data) override;

    // Process-control commands (TCP clients only)
    Q_INVOKABLE void launch(const QString &name);

signals:
    void hostChanged();
    void portChanged();
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
    QByteArray  m_buffer;
    QString     m_host = QStringLiteral("127.0.0.1");
    int         m_port = 49152;
    QString     m_name;
    bool        m_registered = false;
};
