#include <QtTest>
#include <QSignalSpy>
#include <QJsonObject>
#include <QPluginLoader>
#include <QLibrary>
#include <QDir>
#include <QFileInfo>

// Pull in the header-only client directly — no ConcertoBus lib dependency.
#include "RawBusClient.h"

// We spin up a real BusCore in-process so the test is self-contained.
#include "BusCore.h"
#include "IBusTransport.h"

class TstQt5Client : public QObject
{
    Q_OBJECT

private:
    // Plugin is loaded once for all tests and kept alive.
    QPluginLoader m_loader;
    IBusTransport *m_factory = nullptr;

    // Per-test state.
    BusCore *m_core = nullptr;
    IBusTransport *m_transport = nullptr;
    quint16 m_port = 0;

    // Probe both CMake multi-config layout (plugins/<config>/TcpTransport)
    // and qmake single-config layout (plugins/TcpTransport).
    static QString tcpPluginPath() {
        QDir appDir(QCoreApplication::applicationDirPath());
        const QString config = appDir.dirName(); // e.g. "RelWithDebInfo" or "debug"

        // CMake/MSVC: test lives in <build>/<config>/, plugin in <build>/plugins/<config>/
        const QString multiConfig = appDir.absoluteFilePath(
            QString("../plugins/%1/TcpTransport").arg(config));
        if (QLibrary::isLibrary(multiConfig + ".dll") ||
            QLibrary::isLibrary(multiConfig + ".so")  ||
            QFileInfo::exists(multiConfig + ".dll")   ||
            QFileInfo::exists(multiConfig + ".so"))
            return multiConfig;

        // qmake: test lives in <build>/tests/, plugin in <build>/plugins/
        return appDir.absoluteFilePath("../plugins/TcpTransport");
    }

private slots:
    void initTestCase() {
        m_loader.setFileName(tcpPluginPath());
        m_factory = qobject_cast<IBusTransport *>(m_loader.instance());
        QVERIFY2(m_factory, qPrintable(
            QString("Cannot load TcpTransport plugin from %1: %2")
                .arg(tcpPluginPath(), m_loader.errorString())));
    }

    void cleanupTestCase() {
        // Keep plugin loaded until process exit to avoid unload/reload race.
    }

    void init() {
        // Create a fresh transport instance (not the plugin root) per test.
        // Parent is nullptr; we delete it explicitly in cleanup().
        m_transport = m_factory->createInstance();
        QVariantMap cfg;
        cfg[QStringLiteral("port")] = 0;  // OS picks a free port
        QVERIFY(m_transport->start(cfg));
        m_port = m_transport->property("port").value<quint16>();
        QVERIFY(m_port != 0);

        m_core = new BusCore(this);
        m_core->addTransport(m_transport);
    }

    void cleanup() {
        // BusCore took ownership of m_transport via addTransport(reparent=true),
        // so deleting m_core also deletes m_transport.
        delete m_core;
        m_core = nullptr;
        m_transport = nullptr;
    }

    void test_registerAndReceive()
    {
        RawBusClient sender("Sender", "127.0.0.1", m_port);
        RawBusClient receiver("Receiver", "127.0.0.1", m_port);

        QSignalSpy senderReady  (&sender,   &RawBusClient::connected);
        QSignalSpy receiverReady(&receiver, &RawBusClient::connected);
        QSignalSpy msgSpy       (&receiver, &RawBusClient::messageReceived);

        // Connect sequentially: complete each register handshake before the next.
        sender.connectToBus();
        QVERIFY(senderReady.wait(5000));
        receiver.connectToBus();
        QVERIFY(receiverReady.wait(5000));

        receiver.subscribe("data");
        QTest::qWait(50);

        sender.publish("data", QJsonObject{{"x", 99}});

        QVERIFY(msgSpy.wait(5000));
        QCOMPARE(msgSpy.size(), 1);
        QCOMPARE(msgSpy[0][0].toString(), QStringLiteral("data"));
        QCOMPARE(msgSpy[0][1].toString(), QStringLiteral("Sender"));
        QCOMPARE(msgSpy[0][2].value<QJsonObject>()["x"].toInt(), 99);
    }

    void test_offlineQueue()
    {
        RawBusClient producer("Producer", "127.0.0.1", m_port);
        QSignalSpy prodReady(&producer, &RawBusClient::connected);
        producer.connectToBus();
        QVERIFY(prodReady.wait(5000));

        producer.publish("queue", QJsonObject{{"msg", "hello"}});
        QTest::qWait(50);

        RawBusClient consumer("Consumer", "127.0.0.1", m_port);
        QSignalSpy consReady(&consumer, &RawBusClient::connected);
        QSignalSpy msgSpy   (&consumer, &RawBusClient::messageReceived);
        consumer.connectToBus();
        QVERIFY(consReady.wait(5000));

        consumer.subscribe("queue");

        QVERIFY(msgSpy.wait(5000));
        QCOMPARE(msgSpy[0][2].value<QJsonObject>()["msg"].toString(),
                 QStringLiteral("hello"));
    }

    void test_duplicateName()
    {
        RawBusClient first ("App", "127.0.0.1", m_port);
        RawBusClient second("App", "127.0.0.1", m_port);

        QSignalSpy firstReady (&first,  &RawBusClient::connected);
        QSignalSpy secondError(&second, &RawBusClient::errorOccurred);

        first.connectToBus();
        QVERIFY(firstReady.wait(5000));

        second.connectToBus();
        QVERIFY(secondError.wait(5000));
        QCOMPARE(secondError[0][0].toString(), QStringLiteral("name_taken"));
    }
};

QTEST_MAIN(TstQt5Client)
#include "tst_qt5client.moc"
