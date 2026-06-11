#include <QtTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QProcess>

#include "BusCore.h"
#include "ProcessManager.h"

// Path to the echo helper built alongside the tests
static QString helperPath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/stdio_echo_helper.exe");
}

// Minimal fake transport that lets a test act as a connected client.
// Call injectJson() to deliver a message into BusCore as if it came from
// clientId, and collect replies via m_sent.
class FakeTransport : public IBusTransport
{
    Q_OBJECT
public:
    explicit FakeTransport(QObject *parent = nullptr) : IBusTransport(parent) {}

    bool start(const QVariantMap &) override { return true; }
    void send(ClientId id, const QByteArray &json) override { m_sent.append(qMakePair(id, json)); }
    void closeClient(ClientId) override {}

    void connectClient(ClientId id) { emit clientConnected(id); }
    void disconnectClient(ClientId id) { emit clientDisconnected(id); }
    void injectJson(ClientId id, const QByteArray &json) { emit messageReceived(id, json); }

    // Returns the first reply sent to id, parsed; removes it.
    QJsonObject takeFirst(ClientId id) {
        for (int i = 0; i < m_sent.size(); ++i) {
            if (m_sent[i].first == id) {
                QJsonObject obj = QJsonDocument::fromJson(m_sent[i].second).object();
                m_sent.removeAt(i);
                return obj;
            }
        }
        return {};
    }

    QList<QJsonObject> drain(ClientId id) {
        QList<QJsonObject> result;
        for (int i = 0; i < m_sent.size(); ) {
            if (m_sent[i].first == id) {
                result.append(QJsonDocument::fromJson(m_sent[i].second).object());
                m_sent.removeAt(i);
            } else {
                ++i;
            }
        }
        return result;
    }

    QList<QPair<ClientId, QByteArray>> m_sent;
};

// Helper: register a fake client and return the reply
static QJsonObject registerClient(FakeTransport *t, BusCore &core, ClientId id, const QString &name)
{
    Q_UNUSED(core)
    t->connectClient(id);
    t->injectJson(id, QJsonDocument(QJsonObject{
        {QStringLiteral("cmd"),  QStringLiteral("register")},
        {QStringLiteral("name"), name}
    }).toJson(QJsonDocument::Compact));
    return t->takeFirst(id);
}

class TstBusCore : public QObject
{
    Q_OBJECT

private slots:
    void test_launchUnknownProcess()
    {
        BusCore core;
        ProcessManager pm;
        core.setProcessManager(&pm);

        FakeTransport *ft = new FakeTransport(&core);
        core.addTransport(ft);

        constexpr ClientId cid = 0xF001;
        QCOMPARE(registerClient(ft, core, cid, "Ctrl")["ok"].toBool(), true);

        ft->injectJson(cid, QJsonDocument(QJsonObject{
            {QStringLiteral("cmd"),  QStringLiteral("launch")},
            {QStringLiteral("name"), QStringLiteral("NoSuchProcess")}
        }).toJson(QJsonDocument::Compact));

        QJsonObject reply = ft->takeFirst(cid);
        QCOMPARE(reply[QStringLiteral("error")].toString(), QStringLiteral("unknown_process"));
    }

    void test_launchAlreadyRunning()
    {
        // If the target process is already registered in the Router,
        // launch should immediately reply process_started.
        BusCore core;
        ProcessManager pm;
        pm.addEntry("Echo", "cmd.exe", {"/c", "exit 0"});
        core.setProcessManager(&pm);

        FakeTransport *ft = new FakeTransport(&core);
        core.addTransport(ft);

        // Pre-register "Echo" directly in the router (simulates it already being up)
        constexpr ClientId echoCid = 42;
        core.router()->handleRegister(echoCid, QStringLiteral("Echo"));

        constexpr ClientId cid = 0xF001;
        QCOMPARE(registerClient(ft, core, cid, "Ctrl")["ok"].toBool(), true);

        ft->injectJson(cid, QJsonDocument(QJsonObject{
            {QStringLiteral("cmd"),  QStringLiteral("launch")},
            {QStringLiteral("name"), QStringLiteral("Echo")}
        }).toJson(QJsonDocument::Compact));

        QJsonObject reply = ft->takeFirst(cid);
        QCOMPARE(reply[QStringLiteral("event")].toString(), QStringLiteral("process_started"));
    }

    void test_launchReceivesStartedThenStopped()
    {
        if (!QFile::exists(helperPath()))
            QSKIP("stdio_echo_helper.exe not found beside test exe");

        BusCore core;
        ProcessManager pm;
        pm.addEntry("Echo", helperPath(), {});
        core.setProcessManager(&pm);

        FakeTransport *ft = new FakeTransport(&core);
        core.addTransport(ft);

        constexpr ClientId cid = 0xF001;
        QCOMPARE(registerClient(ft, core, cid, "Ctrl")["ok"].toBool(), true);

        // Send launch command
        ft->injectJson(cid, QJsonDocument(QJsonObject{
            {QStringLiteral("cmd"),  QStringLiteral("launch")},
            {QStringLiteral("name"), QStringLiteral("Echo")}
        }).toJson(QJsonDocument::Compact));

        // Immediate reply should be "launching"
        QJsonObject reply = ft->takeFirst(cid);
        QCOMPARE(reply[QStringLiteral("event")].toString(), QStringLiteral("launching"));

        // Wait for process_started notification (Echo registers itself)
        QSignalSpy spy(&core, &BusCore::clientRegistered);
        QVERIFY(spy.wait(5000));

        // Should now have a process_started push for the watcher
        QList<QJsonObject> msgs = ft->drain(cid);
        bool gotStarted = false;
        for (const auto &m : msgs)
            if (m[QStringLiteral("event")].toString() == QLatin1String("process_started"))
                gotStarted = true;
        QVERIFY(gotStarted);

        // Kill — QProcess::kill() causes CrashExit on Windows, so processCrashed fires.
        // Create spy before issuing the kill command.
        QSignalSpy crashedSpy(&pm, &ProcessManager::processCrashed);
        ft->injectJson(cid, QJsonDocument(QJsonObject{
            {QStringLiteral("cmd"),  QStringLiteral("kill")},
            {QStringLiteral("name"), QStringLiteral("Echo")}
        }).toJson(QJsonDocument::Compact));
        QVERIFY(!crashedSpy.isEmpty() || crashedSpy.wait(3000));

        // BusCore should push process_crashed to the watcher
        QList<QJsonObject> afterKill = ft->drain(cid);
        bool gotCrashed = false;
        for (const auto &m : afterKill)
            if (m[QStringLiteral("event")].toString() == QLatin1String("process_crashed"))
                gotCrashed = true;
        QVERIFY(gotCrashed);
    }

    void test_doubleLaunch()
    {
        if (!QFile::exists(helperPath()))
            QSKIP("stdio_echo_helper.exe not found beside test exe");

        BusCore core;
        ProcessManager pm;
        pm.addEntry("Echo", helperPath(), {});
        core.setProcessManager(&pm);

        FakeTransport *ft = new FakeTransport(&core);
        core.addTransport(ft);

        constexpr ClientId cid1 = 0xF001, cid2 = 0xF002;
        QCOMPARE(registerClient(ft, core, cid1, "Ctrl1")["ok"].toBool(), true);
        QCOMPARE(registerClient(ft, core, cid2, "Ctrl2")["ok"].toBool(), true);

        auto launchCmd = QJsonDocument(QJsonObject{
            {QStringLiteral("cmd"),  QStringLiteral("launch")},
            {QStringLiteral("name"), QStringLiteral("Echo")}
        }).toJson(QJsonDocument::Compact);

        ft->injectJson(cid1, launchCmd);
        ft->injectJson(cid2, launchCmd);

        // Both get immediate replies
        QJsonObject r1 = ft->takeFirst(cid1);
        QJsonObject r2 = ft->takeFirst(cid2);
        // First: launching; second: may be launching or process_started depending on timing
        QVERIFY(r1[QStringLiteral("event")] == QLatin1String("launching") ||
                r1[QStringLiteral("event")] == QLatin1String("process_started"));
        QVERIFY(r2[QStringLiteral("event")] == QLatin1String("launching") ||
                r2[QStringLiteral("event")] == QLatin1String("process_started"));

        // Wait for the process to register
        QSignalSpy spy(&core, &BusCore::clientRegistered);
        if (spy.isEmpty()) QVERIFY(spy.wait(5000));

        // Both watchers should receive process_started
        QList<QJsonObject> msgs1 = ft->drain(cid1);
        QList<QJsonObject> msgs2 = ft->drain(cid2);
        bool s1 = false, s2 = false;
        for (const auto &m : msgs1)
            if (m[QStringLiteral("event")].toString() == QLatin1String("process_started")) s1 = true;
        for (const auto &m : msgs2)
            if (m[QStringLiteral("event")].toString() == QLatin1String("process_started")) s2 = true;
        QVERIFY(s1);
        QVERIFY(s2);

        // Process was started only once
        QCOMPARE(pm.names().size(), 1);
        QVERIFY(pm.isRunning("Echo"));

        pm.kill("Echo");
    }
};

QTEST_MAIN(TstBusCore)
#include "tst_buscore.moc"
