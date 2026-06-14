#include <QtTest>
#include <QSignalSpy>
#include <QJsonObject>
#include <QPluginLoader>

// Pull in the header-only client directly — no ConcertoBus lib dependency.
#include "RawBusClient.h"

// We spin up a real BusCore in-process so the test is self-contained.
#include "BusCore.h"
#include "IBusTransport.h"

class TstQt5Client : public QObject
{
    Q_OBJECT

private:
    BusCore *m_core = nullptr;
    quint16  m_port = 0;
    QPluginLoader m_loader;

    // Plugin is built into plugins/<config>/ beside the test binary's <config>/ dir.
    static QString tcpPluginPath() {
        QDir appDir(QCoreApplication::applicationDirPath());
        const QString config = appDir.dirName(); // e.g. "RelWithDebInfo"
        return appDir.absoluteFilePath(
            QString("../plugins/%1/TcpTransport").arg(config));
    }

    void startCore() {
        // Load the TCP transport plugin.
        m_loader.setFileName(tcpPluginPath());
        auto *transport = qobject_cast<IBusTransport *>(m_loader.instance());
        QVERIFY2(transport, qPrintable(m_loader.errorString()));

        // port 0 → OS picks a free port.
        QVariantMap cfg;
        cfg[QStringLiteral("port")] = 0;
        QVERIFY(transport->start(cfg));

        // Read back the actual port via the Q_PROPERTY exposed by TcpTransport.
        m_port = transport->property("port").value<quint16>();
        QVERIFY(m_port != 0);

        m_core = new BusCore(this);
        m_core->addTransport(transport);
    }

private slots:
    void init()    { startCore(); }
    void cleanup() {
        delete m_core; m_core = nullptr;
        m_loader.unload();
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
