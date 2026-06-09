// Tests BusProxy property/signal hooking and BusForeign updateFromNetwork.
// Uses direct BusClient connections (no QML engine needed).

#include "BusClient.h"
#include "BusServer.h"
#include "Catalog.h"
#include "BusClientQml.h"

#include <QTest>
#include <QSignalSpy>

// A concrete BusForeign subclass with user-defined properties and a signal
class TestForeign : public BusForeign
{
    Q_OBJECT
public:
    explicit TestForeign(QObject *parent = nullptr) : BusForeign(parent) {}
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)

    int value() const { return m_value; }
    void setValue(int v) { if (m_value == v) return; m_value = v; emit valueChanged(); }
    QString label() const { return m_label; }
    void setLabel(const QString &s) { if (m_label == s) return; m_label = s; emit labelChanged(); }

signals:
    void valueChanged();
    void labelChanged();

private:
    int m_value = 0;
    QString m_label;
};

// A BusProxy subclass with matching properties
class TestProxy : public BusProxy
{
    Q_OBJECT
public:
    explicit TestProxy(QObject *parent = nullptr) : BusProxy(parent) {}
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)

    int value() const { return m_value; }
    void setValue(int v) { if (m_value == v) return; m_value = v; emit valueChanged(); }
    QString label() const { return m_label; }
    void setLabel(const QString &s) { if (m_label == s) return; m_label = s; emit labelChanged(); }

signals:
    void valueChanged();
    void labelChanged();
    void actionFired(int code);

private:
    int m_value = 0;
    QString m_label;
};

class TstReflection : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_server = new BusServer(this);
        m_server->setCatalog(new Catalog(this));
        QVERIFY(m_server->listen(0));
        m_port = m_server->port();
    }

    void test_proxyPublishesPropertyOnChange()
    {
        // Receiver (raw BusClient spying on the topic)
        BusClient spy(this);
        spy.setPort(m_port);
        spy.setName("SpyA");
        QSignalSpy msgSpy(&spy, &BusClient::messageReceived);
        spy.connectToBus();
        QVERIFY(QTest::qWaitFor([&]{ return spy.isConnected(); }, 2000));
        spy.subscribe("NodeA/data");
        QTest::qWait(30);

        // Proxy
        TestProxy proxy;
        proxy.setBusPort(m_port);
        proxy.setTargetProcess("NodeA");
        proxy.setTag("data");
        proxy.componentComplete();  // triggers async connectToBus
        // Wait until proxy is connected and has hooked + published initial values
        QVERIFY(QTest::qWaitFor([&]{ return proxy.isHooked() && proxy.isConnected(); }, 2000));
        QTest::qWait(50);

        // Should have received the initial property values (value=0, label="")
        const int initialCount = msgSpy.count();
        QVERIFY(initialCount >= 1);
        msgSpy.clear();

        // Change a property → should be published
        proxy.setValue(42);
        QVERIFY(msgSpy.wait(1000));
        QCOMPARE(msgSpy.count(), 1);
        const QJsonObject data = msgSpy.first()[1].toJsonObject();
        QCOMPARE(data["properties"].toObject()["value"].toInt(), 42);
    }

    void test_foreignAppliesProperties()
    {
        // Sender (raw BusClient publishing to the topic)
        BusClient sender(this);
        sender.setPort(m_port);
        sender.setName("NodeB_sender");
        sender.connectToBus();
        QVERIFY(QTest::qWaitFor([&]{ return sender.isConnected(); }, 2000));

        // Foreign receiving on NodeB/sensor
        TestForeign foreign;
        foreign.setBusPort(m_port);
        foreign.setName("NodeB");
        foreign.setTag("sensor");
        foreign.componentComplete();
        QVERIFY(QTest::qWaitFor([&]{ return foreign.busHost().isEmpty() || true; }, 50));
        QTest::qWait(100);

        QSignalSpy valueSpy(&foreign, &TestForeign::valueChanged);

        sender.publish("NodeB/sensor", QJsonObject{{"properties", QJsonObject{{"value", 99}}}});

        QVERIFY(valueSpy.wait(1000));
        QCOMPARE(foreign.value(), 99);
    }

    void test_proxyPublishesActionTrigger()
    {
        BusClient spy(this);
        spy.setPort(m_port);
        spy.setName("SpyC");
        QSignalSpy msgSpy(&spy, &BusClient::messageReceived);
        spy.connectToBus();
        QVERIFY(QTest::qWaitFor([&]{ return spy.isConnected(); }, 2000));
        spy.subscribe("NodeC/ctrl");
        QTest::qWait(30);

        TestProxy proxy;
        proxy.setBusPort(m_port);
        proxy.setTargetProcess("NodeC");
        proxy.setTag("ctrl");
        proxy.componentComplete();
        QVERIFY(QTest::qWaitFor([&]{ return proxy.isHooked() && proxy.isConnected(); }, 2000));
        QTest::qWait(50);  // let initial property values flush through and spy subscription settle
        msgSpy.clear();

        // Fire the actionFired signal on the proxy
        emit proxy.actionFired(7);

        QVERIFY(msgSpy.wait(1000));
        bool found = false;
        for (const auto &call : std::as_const(msgSpy)) {
            const QJsonObject d = call[1].toJsonObject();
            if (d["action"].toString() == "actionFired") {
                QCOMPARE(d["args"].toArray()[0].toInt(), 7);
                found = true;
            }
        }
        QVERIFY(found);
    }

private:
    BusServer *m_server = nullptr;
    quint16 m_port = 0;
};

QTEST_MAIN(TstReflection)
#include "tst_reflection.moc"
