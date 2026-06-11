#include <QtTest>
#include <QSignalSpy>

#include "../bus/ProcessManager.h"

// Suppress QProcess warnings about processes that exit immediately during
// autoRestart tests (e.g. "Executable path is empty", "QProcess: Destroyed
// while process … is still running"). These are expected in the test context.
static QtMessageHandler g_prevHandler = nullptr;
static void silentQProcessWarnings(QtMsgType type, const QMessageLogContext &ctx,
                                   const QString &msg)
{
    if (msg.contains(QLatin1String("QProcess")) || msg.contains(QLatin1String("Executable path")))
        return;
    if (g_prevHandler) g_prevHandler(type, ctx, msg);
    else if (type >= QtWarningMsg) fprintf(stderr, "%s\n", qPrintable(msg));
}

class TstProcessManager : public QObject
{
    Q_OBJECT

private slots:
    // Install AFTER QTest::qExec() has set up its own handler; restore on teardown.
    // Q_CONSTRUCTOR_FUNCTION runs too early — QTest replaces any handler installed
    // before qExec(), so the filter must be (re-)installed here.
    void initTestCase()
    {
        g_prevHandler = qInstallMessageHandler(silentQProcessWarnings);
    }
    void cleanupTestCase()
    {
        qInstallMessageHandler(g_prevHandler);
        g_prevHandler = nullptr;
    }
    void test_loadFromConfig()
    {
        ProcessManager mgr;
        mgr.addEntry("TestApp", "cmd.exe", {"/c", "exit 0"},
                     {}, {"events"}, "tcp");

        QVERIFY(mgr.names().contains("TestApp"));
        QCOMPARE(mgr.subscriptionsFor("TestApp"), QStringList{"events"});
        QCOMPARE(mgr.transportFor("TestApp"), QStringLiteral("tcp"));
    }

    void test_launchAndStop()
    {
        ProcessManager mgr;
        // ping -n 10 runs for ~10 seconds
        mgr.addEntry("Ping", "ping.exe", {"-n", "10", "127.0.0.1"});

        QSignalSpy startedSpy(&mgr, &ProcessManager::processStarted);
        QSignalSpy stoppedSpy(&mgr, &ProcessManager::processStopped);

        QVERIFY(mgr.launch("Ping"));
        // Signal may have already fired synchronously during launch()'s waitForStarted()
        QVERIFY(!startedSpy.isEmpty() || startedSpy.wait(3000));
        QVERIFY(mgr.isRunning("Ping"));

        mgr.kill("Ping");
        // Same pattern for stopped
        QVERIFY(!stoppedSpy.isEmpty() || stoppedSpy.wait(3000) || !mgr.isRunning("Ping"));
        QVERIFY(!mgr.isRunning("Ping"));
    }

    void test_autoRestart()
    {
        ProcessManager mgr;
        // cmd.exe exits immediately — autoRestart should relaunch up to maxRestarts times
        mgr.addEntry("Crasher", "cmd.exe", {"/c", "exit 0"},
                     {}, {}, "stdio",
                     /*autoRestart=*/true, /*restartDelayMs=*/200, /*maxRestarts=*/2);

        QSignalSpy stoppedSpy(&mgr, &ProcessManager::processStopped);
        QSignalSpy startedSpy(&mgr, &ProcessManager::processStarted);

        mgr.launch("Crasher");

        // initial + up to 2 restarts = up to 3 starts/stops total
        QTest::qWait(2000);

        int starts = startedSpy.count();
        int stops  = stoppedSpy.count();
        QVERIFY2(starts >= 1, qPrintable(QString("Expected >=1 starts, got %1").arg(starts)));
        QVERIFY2(stops  >= 1, qPrintable(QString("Expected >=1 stops, got %1").arg(stops)));
        QVERIFY2(starts <= 3, qPrintable(QString("Expected <=3 starts (maxRestarts=2), got %1").arg(starts)));
    }

    void test_unknownName()
    {
        ProcessManager mgr;
        QVERIFY(!mgr.launch("nobody"));
        mgr.kill("nobody");     // must not crash
        mgr.restart("nobody");  // must not crash
        QVERIFY(!mgr.isRunning("nobody"));
    }

    void test_subscriptionsAndTransport()
    {
        ProcessManager mgr;
        mgr.addEntry("Worker", "cmd.exe", {"/c", "exit 0"},
                     {}, {"jobs", "status"}, "stdio");

        QCOMPARE(mgr.subscriptionsFor("Worker"), QStringList({"jobs", "status"}));
        QCOMPARE(mgr.transportFor("Worker"), QStringLiteral("stdio"));
    }
};

QTEST_MAIN(TstProcessManager)
#include "tst_processmanager.moc"
