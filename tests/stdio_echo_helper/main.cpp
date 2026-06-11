// Echo helper for tst_stdio.
// Reads JSON lines from stdin using a background QThread, registers as "Echo",
// subscribes to "ping", and re-publishes every received push to "pong".
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QFile>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>

#ifdef Q_OS_WIN
#  include <io.h>
#  include <fcntl.h>
#  include <windows.h>
#endif

// ── Stdin reader thread ────────────────────────────────────────────────────────
class StdinThread : public QThread
{
    Q_OBJECT
protected:
    void run() override
    {
#ifdef Q_OS_WIN
        _setmode(_fileno(stdin), _O_BINARY);
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
#endif
        QByteArray buf;
        while (true) {
#ifdef Q_OS_WIN
            // Block until data is available or pipe closes
            char tmp[4096];
            DWORD read = 0;
            if (!ReadFile(h, tmp, sizeof(tmp), &read, nullptr) || read == 0) {
                emit eof();
                return;
            }
            buf.append(tmp, static_cast<int>(read));
#else
            QFile f;
            f.open(stdin, QIODevice::ReadOnly | QIODevice::Unbuffered);
            QByteArray chunk = f.read(4096);
            if (chunk.isEmpty()) { emit eof(); return; }
            buf += chunk;
#endif
            int nl;
            while ((nl = buf.indexOf('\n')) >= 0) {
                QByteArray line = buf.left(nl).trimmed();
                buf.remove(0, nl + 1);
                if (!line.isEmpty())
                    emit lineReceived(line);
            }
        }
    }
signals:
    void lineReceived(const QByteArray &line);
    void eof();
};

// ── Main ───────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

#ifdef Q_OS_WIN
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    QFile out;
    out.open(stdout, QIODevice::WriteOnly | QIODevice::Unbuffered);

    auto send = [&](const QJsonObject &obj) {
        out.write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
        out.flush();
    };

    auto *reader = new StdinThread;

    QObject::connect(reader, &StdinThread::lineReceived, &app,
                     [&](const QByteArray &line) {
        QJsonObject obj = QJsonDocument::fromJson(line).object();

        if (obj[QStringLiteral("push")].toBool()) {
            // Re-publish payload to "pong"
            send(QJsonObject{
                {QStringLiteral("cmd"),  QStringLiteral("publish")},
                {QStringLiteral("to"),   QStringLiteral("pong")},
                {QStringLiteral("data"), obj[QStringLiteral("data")]}
            });
        } else if (obj[QStringLiteral("ok")].toBool()) {
            // Registration ack — subscribe to ping
            send(QJsonObject{
                {QStringLiteral("cmd"), QStringLiteral("subscribe")},
                {QStringLiteral("tag"), QStringLiteral("ping")}
            });
        }
    }, Qt::QueuedConnection);

    QObject::connect(reader, &StdinThread::eof, &app, &QCoreApplication::quit,
                     Qt::QueuedConnection);

    reader->start();

    // Register immediately
    send(QJsonObject{
        {QStringLiteral("cmd"),  QStringLiteral("register")},
        {QStringLiteral("name"), QStringLiteral("Echo")}
    });

    return app.exec();
}

#include "main.moc"
