#include "BusClient.h"

#include <QJsonDocument>
#include <QTcpSocket>
#include <QDebug>

BusClient::BusClient(QObject *parent)
    : QObject(parent)
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
    // Re-register if already connected
    if (isConnected() && !n.isEmpty())
        sendCmd(QJsonObject{{"cmd","register"},{"name",n}});
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

void BusClient::subscribe(const QString &topic)
{
    sendCmd(QJsonObject{{"cmd","subscribe"},{"to",topic}});
}

void BusClient::unsubscribe(const QString &topic)
{
    sendCmd(QJsonObject{{"cmd","unsubscribe"},{"to",topic}});
}

void BusClient::publish(const QString &topic, const QJsonObject &data)
{
    QJsonObject cmd;
    cmd["cmd"]  = "publish";
    cmd["to"]   = topic;
    cmd["data"] = data;
    sendCmd(cmd);
}

void BusClient::launch(const QString &name)
{
    sendCmd(QJsonObject{{"cmd","launch"},{"name",name}});
}

void BusClient::onConnected()
{
    emit connectedChanged();
    if (!m_name.isEmpty()) {
        sendCmd(QJsonObject{{"cmd","register"},{"name",m_name}});
        m_registered = true;
    }
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

void BusClient::onError(QAbstractSocket::SocketError err)
{
    qWarning() << "BusClient: socket error" << err << m_socket->errorString();
}

void BusClient::sendCmd(const QJsonObject &cmd)
{
    if (!isConnected()) {
        qWarning() << "BusClient: not connected, dropping cmd" << cmd["cmd"].toString();
        return;
    }
    QByteArray line = QJsonDocument(cmd).toJson(QJsonDocument::Compact);
    line += '\n';
    m_socket->write(line);
}

void BusClient::processLine(const QByteArray &line)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "BusClient: bad JSON from bus:" << line;
        return;
    }
    const QJsonObject msg = doc.object();

    if (msg.contains("push")) {
        emit messageReceived(msg["from"].toString(), msg["data"].toObject());
        return;
    }
    if (msg.contains("status")) {
        const QString status = msg["status"].toString();
        if (status == "ready")
            emit launchReady(msg["name"].toString());
        return;
    }
    if (msg.contains("error")) {
        const QString e = msg["error"].toString();
        // Propagate launch errors
        if (e == "not_found" || e == "launch_failed")
            emit launchError(QString(), e);
        else
            qWarning() << "BusClient: bus error:" << e;
        return;
    }
    // "ok" acks — silently ignored
}
