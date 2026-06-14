#include "BusClient.h"

#include <QJsonDocument>
#include <QTcpSocket>

BusClient::BusClient(QObject *parent)
    : AbstractBusClient(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected,    this, &BusClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &BusClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,    this, &BusClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &BusClient::onError);
}

void BusClient::setHost(const QString &h)
{
    if (m_host == h) return;
    m_host = h;
    emit hostChanged();
}

void BusClient::setPort(int p)
{
    if (m_port == p) return;
    m_port = p;
    emit portChanged();
}

void BusClient::setName(const QString &n)
{
    if (m_name == n) return;
    m_name = n;
    emit nameChanged();
    if (isConnected() && !n.isEmpty())
        sendCmd(QJsonObject{{QStringLiteral("cmd"), QStringLiteral("register")},
                            {QStringLiteral("name"), n}});
}

bool BusClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void BusClient::connectToBus()
{
    if (isConnected()) return;
    m_socket->connectToHost(m_host, static_cast<quint16>(m_port));
}

void BusClient::subscribe(const QString &tag)
{
    sendCmd(QJsonObject{{QStringLiteral("cmd"), QStringLiteral("subscribe")},
                        {QStringLiteral("tag"), tag}});
}

void BusClient::unsubscribe(const QString &tag)
{
    sendCmd(QJsonObject{{QStringLiteral("cmd"), QStringLiteral("unsubscribe")},
                        {QStringLiteral("tag"), tag}});
}

void BusClient::publish(const QString &to, const QJsonObject &data)
{
    sendCmd(QJsonObject{{QStringLiteral("cmd"),  QStringLiteral("publish")},
                        {QStringLiteral("to"),   to},
                        {QStringLiteral("data"), data}});
}

void BusClient::launch(const QString &name)
{
    sendCmd(QJsonObject{{QStringLiteral("cmd"),  QStringLiteral("launch")},
                        {QStringLiteral("name"), name}});
}

void BusClient::onConnected()
{
    if (!m_name.isEmpty()) {
        sendCmd(QJsonObject{{QStringLiteral("cmd"),  QStringLiteral("register")},
                            {QStringLiteral("name"), m_name}});
    }
    // connectedChanged() is emitted from processLine() on {"ok":true}
}

void BusClient::onDisconnected()
{
    m_registered = false;
    emit connectedChanged();
}

void BusClient::onReadyRead()
{
    m_buffer += m_socket->readAll();
    int nl;
    while ((nl = m_buffer.indexOf('\n')) != -1) {
        const QByteArray line = m_buffer.left(nl).trimmed();
        m_buffer.remove(0, nl + 1);
        if (!line.isEmpty())
            processLine(line);
    }
}

void BusClient::onError(QAbstractSocket::SocketError)
{
    emit errorOccurred(m_socket->errorString());
}

void BusClient::sendCmd(const QJsonObject &cmd)
{
    if (!isConnected()) return;
    m_socket->write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n');
}

void BusClient::processLine(const QByteArray &line)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    const QJsonObject msg = doc.object();

    if (msg[QStringLiteral("push")].toBool()) {
        emit messageReceived(
            msg[QStringLiteral("from")].toString(),
            msg[QStringLiteral("sender")].toString(),
            msg[QStringLiteral("data")].toObject());
        return;
    }
    if (msg.contains(QStringLiteral("event"))) {
        const QString event = msg[QStringLiteral("event")].toString();
        const QString name  = msg[QStringLiteral("name")].toString();
        if (event == QLatin1String("process_started"))
            emit launchReady(name);
        else if (event == QLatin1String("process_stopped") || event == QLatin1String("process_crashed"))
            emit launchError(name, event);
        // "launching" is informational — silently ignored
        return;
    }
    if (msg.contains(QStringLiteral("error"))) {
        const QString e = msg[QStringLiteral("error")].toString();
        if (e == QLatin1String("unknown_process") || e == QLatin1String("launch_failed"))
            emit launchError(msg[QStringLiteral("name")].toString(), e);
        else
            emit errorOccurred(e);
        return;
    }
    // {"ok":true} is the register acknowledgement
    if (msg[QStringLiteral("ok")].toBool() && !m_registered) {
        m_registered = true;
        emit connectedChanged();
    }
}
