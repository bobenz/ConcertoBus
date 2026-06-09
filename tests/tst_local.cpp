#include "BusClient.h"
#include "BusServer.h"
#include "Router.h"
#include "Catalog.h"

#include <QTest>
#include <QSignalSpy>
#include <QTimer>

class TstLocal : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_server = new BusServer(this);
        m_server->setCatalog(new Catalog(this));
        QVERIFY(m_server->listen(0)); // port 0 = OS-assigned
        m_port = m_server->port();
    }

    void test_registerAndPublish()
    {
        BusClient sender(this);
        sender.setPort(m_port);
        sender.setName("Sender");

        BusClient receiver(this);
        receiver.setPort(m_port);
        receiver.setName("Receiver");

        QSignalSpy msgSpy(&receiver, &BusClient::messageReceived);

        // Connect both
        sender.connectToBus();
        receiver.connectToBus();
        QVERIFY(QTest::qWaitFor([&]{ return sender.isConnected() && receiver.isConnected(); }, 2000));

        // Receiver subscribes to Receiver/sensor
        receiver.subscribe("Receiver/sensor");
        QTest::qWait(50);

        // Sender publishes to Receiver/sensor
        const QJsonObject payload{{"properties", QJsonObject{{"value", 42}}}};
        sender.publish("Receiver/sensor", payload);

        QVERIFY(msgSpy.wait(1000));
        QCOMPARE(msgSpy.count(), 1);
        const auto args = msgSpy.first();
        QCOMPARE(args[0].toString(), "Receiver/sensor");
        QCOMPARE(args[1].toJsonObject()["properties"].toObject()["value"].toInt(), 42);
    }

    void test_queueing()
    {
        // Publish before receiver connects → messages queued and delivered on connect
        BusClient sender(this);
        sender.setPort(m_port);
        sender.setName("QSender");
        sender.connectToBus();
        QVERIFY(QTest::qWaitFor([&]{ return sender.isConnected(); }, 2000));

        const QJsonObject payload{{"properties", QJsonObject{{"x", 99}}}};
        sender.publish("QReceiver/data", payload);
        QTest::qWait(50);

        // Now connect receiver
        BusClient receiver(this);
        receiver.setPort(m_port);
        receiver.setName("QReceiver");
        QSignalSpy msgSpy(&receiver, &BusClient::messageReceived);
        receiver.subscribe("QReceiver/data");
        receiver.connectToBus();

        QVERIFY(msgSpy.wait(2000));
        QCOMPARE(msgSpy.count(), 1);
        QCOMPARE(msgSpy.first()[1].toJsonObject()["properties"].toObject()["x"].toInt(), 99);
    }

private:
    BusServer *m_server = nullptr;
    quint16 m_port = 0;
};

QTEST_MAIN(TstLocal)
#include "tst_local.moc"
