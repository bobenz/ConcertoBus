#include "StdioBusClient.h"
#include <QFile>
#include <QJsonDocument>
#include <QThread>

#ifdef Q_OS_WIN
#  include <io.h>
#  include <fcntl.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif

class StdinReader : public QThread
{
    Q_OBJECT
protected:
    void run() override
    {
#ifdef Q_OS_WIN
        _setmode(_fileno(stdin), _O_BINARY);
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        QByteArray buf;
        while (true) {
            char tmp[4096];
            DWORD got = 0;
            if (!ReadFile(h, tmp, sizeof(tmp), &got, nullptr) || got == 0)
                return;
            buf.append(tmp, static_cast<int>(got));
            processBuffer(buf);
        }
#else
        QFile f;
        f.open(stdin, QIODevice::ReadOnly | QIODevice::Unbuffered);
        QByteArray buf;
        while (true) {
            QByteArray chunk = f.read(4096);
            if (chunk.isEmpty()) return;
            buf += chunk;
            processBuffer(buf);
        }
#endif
    }

private:
    void processBuffer(QByteArray &buf)
    {
        int nl;
        while ((nl = buf.indexOf('\n')) >= 0) {
            QByteArray line = buf.left(nl).trimmed();
            buf.remove(0, nl + 1);
            if (!line.isEmpty())
                emit lineReceived(line);
        }
    }

signals:
    void lineReceived(const QByteArray &line);
};

#include "StdioBusClient.moc"

// ── StdioBusClient ─────────────────────────────────────────────────────────────

StdioBusClient::StdioBusClient(QObject *parent) : AbstractBusClient(parent) {}

StdioBusClient::~StdioBusClient()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(500);
        if (m_thread->isRunning()) m_thread->terminate();
    }
}

void StdioBusClient::setName(const QString &name)
{
    if (m_name == name) return;
    m_name = name;
    emit nameChanged();
}

void StdioBusClient::connectToBus()
{
    // If stdin is not a pipe (e.g. launched directly from a terminal),
    // skip the daemon handshake and treat the app as immediately connected.
#ifdef Q_OS_WIN
    const DWORD stdinType = GetFileType(GetStdHandle(STD_INPUT_HANDLE));
    const bool hasPipe = (stdinType == FILE_TYPE_PIPE);
#else
    const bool hasPipe = !isatty(fileno(stdin));
#endif
    if (!hasPipe) {
        m_registered = true;
        emit connectedChanged();
        return;
    }

    startReading();
    if (!m_name.isEmpty())
        sendJson(QJsonObject{{QStringLiteral("cmd"),  QStringLiteral("register")},
                             {QStringLiteral("name"), m_name}});
}

void StdioBusClient::subscribe(const QString &tag)
{
    sendJson(QJsonObject{{QStringLiteral("cmd"), QStringLiteral("subscribe")},
                         {QStringLiteral("tag"), tag}});
}

void StdioBusClient::unsubscribe(const QString &tag)
{
    sendJson(QJsonObject{{QStringLiteral("cmd"), QStringLiteral("unsubscribe")},
                         {QStringLiteral("tag"), tag}});
}

void StdioBusClient::publish(const QString &to, const QJsonObject &data)
{
    sendJson(QJsonObject{{QStringLiteral("cmd"),  QStringLiteral("publish")},
                         {QStringLiteral("to"),   to},
                         {QStringLiteral("data"), data}});
}

void StdioBusClient::sendJson(const QJsonObject &obj)
{
    if (!m_stdout) {
        m_stdout = new QFile(this);
#ifdef Q_OS_WIN
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        Q_UNUSED(m_stdout->open(stdout, QIODevice::WriteOnly | QIODevice::Unbuffered));
    }
    m_stdout->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
    Q_UNUSED(m_stdout->flush());
}

void StdioBusClient::startReading()
{
    if (m_thread) return;
    auto *reader = new StdinReader;
    m_thread = reader;
    connect(reader, &StdinReader::lineReceived, this, &StdioBusClient::processLine,
            Qt::QueuedConnection);
    m_thread->start();
}

void StdioBusClient::processLine(const QByteArray &line)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject obj = doc.object();

    if (obj.contains(QStringLiteral("error"))) {
        emit errorOccurred(obj[QStringLiteral("error")].toString());
        return;
    }

    if (obj[QStringLiteral("ok")].toBool()) {
        if (!m_registered) {
            m_registered = true;
            emit connectedChanged();
        }
        return;
    }

    if (obj[QStringLiteral("push")].toBool()) {
        const QJsonObject data = obj[QStringLiteral("data")].toObject();
        if (data[QStringLiteral("inject")].toBool()) {
            emit injectionRequested(
                data[QStringLiteral("name")].toString(),
                data[QStringLiteral("url")].toString(),
                data[QStringLiteral("source")].toString());
            return;
        }
        emit messageReceived(
            obj[QStringLiteral("from")].toString(),
            obj[QStringLiteral("sender")].toString(),
            data);
    }
}

void StdioBusClient::launch(const QString &name)
{
    sendJson(QJsonObject{{QStringLiteral("cmd"),  QStringLiteral("launch")},
                         {QStringLiteral("name"), name}});
}

void StdioBusClient::injectQml(const QString &target, const QString &name,
                                const QString &url, const QString &source)
{
    QJsonObject cmd;
    cmd[QStringLiteral("cmd")]    = QStringLiteral("inject");
    cmd[QStringLiteral("target")] = target;
    cmd[QStringLiteral("name")]   = name;
    if (!url.isEmpty())    cmd[QStringLiteral("url")]    = url;
    if (!source.isEmpty()) cmd[QStringLiteral("source")] = source;
    sendJson(cmd);
}
