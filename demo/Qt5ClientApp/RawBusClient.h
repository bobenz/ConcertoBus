#pragma once
// Minimal ConcertoBus TCP client — compiles against Qt 5 or Qt 6.
// Zero dependency on the ConcertoBus libraries; uses only QtNetwork.
#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpSocket>

class RawBusClient : public QObject
{
    Q_OBJECT
public:
    explicit RawBusClient(const QString &name,
                          const QString &host = QStringLiteral("127.0.0.1"),
                          quint16 port = 49152,
                          QObject *parent = nullptr)
        : QObject(parent), m_name(name), m_host(host), m_port(port)
        , m_socket(new QTcpSocket(this))
    {
        connect(m_socket, &QTcpSocket::connected,    this, &RawBusClient::onConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &RawBusClient::onDisconnected);
        connect(m_socket, &QTcpSocket::readyRead,    this, &RawBusClient::onReadyRead);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        connect(m_socket, &QAbstractSocket::errorOccurred,
                this, [this](QAbstractSocket::SocketError) {
                    emit errorOccurred(m_socket->errorString());
                });
#else
        connect(m_socket,
                QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
                this, [this](QAbstractSocket::SocketError) {
                    emit errorOccurred(m_socket->errorString());
                });
#endif
    }

    bool isConnected() const { return m_registered; }
    QString name() const { return m_name; }

public slots:
    void connectToBus() { m_socket->connectToHost(m_host, m_port); }

    void subscribe(const QString &tag) {
        sendCmd({{"cmd","subscribe"},{"tag",tag}});
    }
    void unsubscribe(const QString &tag) {
        sendCmd({{"cmd","unsubscribe"},{"tag",tag}});
    }
    void publish(const QString &to, const QJsonObject &data) {
        sendCmd({{"cmd","publish"},{"to",to},{"data",data}});
    }

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &tag, const QString &sender, const QJsonObject &data);
    void errorOccurred(const QString &message);

private slots:
    void onConnected() {
        sendCmd({{"cmd","register"},{"name",m_name}});
    }
    void onDisconnected() {
        m_registered = false;
        emit disconnected();
    }
    void onReadyRead() {
        m_buf += m_socket->readAll();
        int nl;
        while ((nl = m_buf.indexOf('\n')) != -1) {
            const QByteArray line = m_buf.left(nl).trimmed();
            m_buf.remove(0, nl + 1);
            if (!line.isEmpty()) processLine(line);
        }
    }

private:
    void processLine(const QByteArray &line) {
        const QJsonObject msg = QJsonDocument::fromJson(line).object();
        if (msg.value(QStringLiteral("ok")).toBool() && !m_registered) {
            m_registered = true;
            emit connected();
        } else if (msg.value(QStringLiteral("push")).toBool()) {
            emit messageReceived(
                msg.value(QStringLiteral("from")).toString(),
                msg.value(QStringLiteral("sender")).toString(),
                msg.value(QStringLiteral("data")).toObject());
        } else if (msg.contains(QStringLiteral("error"))) {
            emit errorOccurred(msg.value(QStringLiteral("error")).toString());
        }
    }

    void sendCmd(const QJsonObject &obj) {
        if (m_socket->state() != QAbstractSocket::ConnectedState) return;
        m_socket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
    }

    const QString m_name;
    const QString m_host;
    const quint16 m_port;
    QTcpSocket   *m_socket;
    QByteArray    m_buf;
    bool          m_registered = false;
};
