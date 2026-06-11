#include <QtTest/QtTest>
#include "BusCore.h"
#include "StdioTransport.h"
#include <QProcess>
#include <QJsonDocument>
#include <QSignalSpy>

static QString helperPath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/stdio_echo_helper.exe");
}

class TstStdio : public QObject
{
    Q_OBJECT

private slots:
    void echoRoundTrip()
    {
        if (!QFile::exists(helperPath())) {
            QSKIP("stdio_echo_helper.exe not found beside test exe");
        }

        BusCore core;
        Router *router = core.router();

        // Register a fake local client for "Tester"
        constexpr ClientId testerId = 9999;
        QVERIFY(router->handleRegister(testerId, QStringLiteral("Tester")));
        router->handleSubscribe(testerId, QStringLiteral("pong"));

        // Collect pushes to testerId
        QList<QByteArray> delivered;
        connect(router, &Router::sendToClient, this,
                [&](ClientId id, const QByteArray &json) {
            if (id == testerId) delivered.append(json);
        });

        // Launch echo helper
        auto *proc = new QProcess(this);
        proc->setProgram(helperPath());
        proc->setProcessChannelMode(QProcess::SeparateChannels);
        core.stdioTransport()->addProcess(proc);
        proc->start();
        QVERIFY(proc->waitForStarted(3000));

        // Wait for Echo to register, then give it time to send the subscribe command.
        QVERIFY(QTest::qWaitFor([&]{
            return router->isRegistered(QStringLiteral("Echo"));
        }, 3000));
        QTest::qWait(300);  // wait for subscribe to arrive after registration ack

        // Publish ping unicast to Echo
        router->handlePublish(QStringLiteral("ping:Echo"),
                              QStringLiteral("Tester"),
                              QJsonObject{{"msg", "hello"}});

        QVERIFY(QTest::qWaitFor([&]{ return !delivered.isEmpty(); }, 3000));
        QCOMPARE(delivered.size(), 1);

        QJsonObject push = QJsonDocument::fromJson(delivered.first()).object();
        QCOMPARE(push[QStringLiteral("from")].toString(), QStringLiteral("pong"));
        QCOMPARE(push[QStringLiteral("data")].toObject()[QStringLiteral("msg")].toString(),
                 QStringLiteral("hello"));

        proc->terminate();
        proc->waitForFinished(2000);
    }

    void broadcastPingPong()
    {
        if (!QFile::exists(helperPath())) {
            QSKIP("stdio_echo_helper.exe not found beside test exe");
        }

        BusCore core;
        Router *router = core.router();

        constexpr ClientId testerId = 9999;
        QVERIFY(router->handleRegister(testerId, QStringLiteral("Tester")));
        router->handleSubscribe(testerId, QStringLiteral("pong"));

        QList<QByteArray> delivered;
        connect(router, &Router::sendToClient, this,
                [&](ClientId id, const QByteArray &json) {
            if (id == testerId) delivered.append(json);
        });

        auto *proc = new QProcess(this);
        proc->setProgram(helperPath());
        proc->setProcessChannelMode(QProcess::SeparateChannels);
        core.stdioTransport()->addProcess(proc);
        proc->start();
        QVERIFY(proc->waitForStarted(3000));

        // Wait for Echo to register, then extra 200ms for subscribe
        QVERIFY(QTest::qWaitFor([&]{
            return router->isRegistered(QStringLiteral("Echo"));
        }, 3000));
        QTest::qWait(200);

        // Broadcast to all ping subscribers
        router->handlePublish(QStringLiteral("ping"), QStringLiteral("Tester"),
                              QJsonObject{{"v", 1}});

        QVERIFY(QTest::qWaitFor([&]{ return !delivered.isEmpty(); }, 3000));
        QCOMPARE(delivered.size(), 1);

        proc->terminate();
        proc->waitForFinished(2000);
    }
};

QTEST_MAIN(TstStdio)
#include "tst_stdio.moc"
