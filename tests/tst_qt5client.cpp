#include <QtTest>
#include <QSignalSpy>
#include <QJsonObject>

// Pull in the header-only client directly — no ConcertoBus lib dependency.
#include "../../demo/Qt5ClientApp/RawBusClient.h"

// We spin up a real BusCore in-process so the test is self-contained.
#include "BusCore.h"
#include "TcpTransport.h"

class TstQt5Client : public QObject
{
    Q_OBJECT

private:
    BusCore *m_core = nullptr;
    quint16  m_port = 0;

    void startCore() {
        m_core = new BusCore(this);
        auto *tcp = new TcpTransport(this);
        tcp->listen(0);          // OS picks a free port
        m_port = tcp->port();
        m_core->addTransport(tcp);
    }

private slots:
    void init()    { startCore(); }
    void cleanup() { delete m_core; m_core = nullptr; }

    void test_registerAndReceive()
    {
        RawBusClient sender("Sender", "127.0.0.1", m_port);
        RawBusClient receiver("Receiver", "127.0.0.1", m_port);

        QSignalSpy senderReady  (&sender,   &RawBusClient::connected);
        QSignalSpy receiverReady(&receiver, &RawBusClient::connected);
        QSignalSpy msgSpy       (&receiver, &RawBusClient::messageReceived);

        // Connect sequentially: complete each register handshake before the next
        // connectToBus() call so readyRead events don't race across two loops.
        sender.connectToBus();
        QVERIFY(senderReady.wait(5000));
        receiver.connectToBus();
        QVERIFY(receiverReady.wait(5000));

        // Subscribe receiver, then publish from sender
        receiver.subscribe("data");
        QTest::qWait(50);   // let subscribe reach the server

        sender.publish("data", QJsonObject{{"x", 99}});

        QVERIFY(msgSpy.wait(5000));
        QCOMPARE(msgSpy.size(), 1);
        QCOMPARE(msgSpy[0][0].toString(), QStringLiteral("data"));
        QCOMPARE(msgSpy[0][1].toString(), QStringLiteral("Sender"));
        QCOMPARE(msgSpy[0][2].value<QJsonObject>()["x"].toInt(), 99);
    }

    void test_offlineQueue()
    {
        // Publish before subscriber exists — bus must queue and drain on subscribe.
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

        consumer.subscribe("queue");   // should drain the queued message

        QVERIFY(msgSpy.wait(5000));
        QCOMPARE(msgSpy[0][2].value<QJsonObject>()["msg"].toString(),
                 QStringLiteral("hello"));
    }

    void test_duplicateName()
    {
        RawBusClient first ("App", "127.0.0.1", m_port);
        RawBusClient second("App", "127.0.0.1", m_port);   // same name

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
