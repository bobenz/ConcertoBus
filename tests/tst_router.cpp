#include <QtTest/QtTest>
#include "Router.h"

class MockSink : public QObject
{
    Q_OBJECT
public:
    struct Delivery { ClientId id; QByteArray json; };
    QList<Delivery> received;

    void hookRouter(Router *r) {
        connect(r, &Router::sendToClient, this, [this](ClientId id, const QByteArray &j){
            received.append({id, j});
        });
    }

    void clear() { received.clear(); }
};

class TstRouter : public QObject
{
    Q_OBJECT

private slots:
    void registerTwiceSameName()
    {
        Router r;
        QVERIFY(r.handleRegister(1, "Alpha"));
        QVERIFY(!r.handleRegister(2, "Alpha"));   // name taken
        QVERIFY(r.isRegistered("Alpha"));
        QCOMPARE(r.nameOf(1), QString("Alpha"));
    }

    void broadcastToSubscribers()
    {
        Router r;
        MockSink sink;
        sink.hookRouter(&r);

        r.handleRegister(1, "A");
        r.handleRegister(2, "B");
        r.handleSubscribe(1, "sensor");
        r.handleSubscribe(2, "sensor");

        r.handlePublish("sensor", "Publisher", QJsonObject{{"v", 42}});

        QCOMPARE(sink.received.size(), 2);
        QSet<ClientId> ids;
        for (auto &d : sink.received) ids.insert(d.id);
        QVERIFY(ids.contains(1));
        QVERIFY(ids.contains(2));
    }

    void broadcastExplicitStar()
    {
        Router r;
        MockSink sink;
        sink.hookRouter(&r);

        r.handleRegister(1, "A");
        r.handleSubscribe(1, "temp");

        r.handlePublish("temp:*", "Pub", QJsonObject{{"t", 99}});
        QCOMPARE(sink.received.size(), 1);
    }

    void unicastDelivery()
    {
        Router r;
        MockSink sink;
        sink.hookRouter(&r);

        r.handleRegister(1, "Alpha");
        r.handleRegister(2, "Beta");
        r.handleSubscribe(1, "cmd");
        r.handleSubscribe(2, "cmd");

        r.handlePublish("cmd:Alpha", "Sender", QJsonObject{{"x", 1}});
        QCOMPARE(sink.received.size(), 1);
        QCOMPARE(sink.received.first().id, ClientId(1));
    }

    void unicastNotSubscribed_queued()
    {
        Router r;
        MockSink sink;
        sink.hookRouter(&r);

        r.handleRegister(1, "Alpha");
        // Alpha doesn't subscribe yet
        r.handlePublish("cmd:Alpha", "Sender", QJsonObject{{"x", 1}});
        QCOMPARE(sink.received.size(), 0);

        r.handleSubscribe(1, "cmd");   // subscribe drains the queue
        QCOMPARE(sink.received.size(), 1);
    }

    void broadcastQueue_drainedOnSubscribe()
    {
        Router r;
        MockSink sink;
        sink.hookRouter(&r);

        // Publish before any subscriber
        r.handlePublish("data", "Pub", QJsonObject{{"n", 1}});
        r.handlePublish("data", "Pub", QJsonObject{{"n", 2}});

        r.handleRegister(1, "Late");
        r.handleSubscribe(1, "data");

        QCOMPARE(sink.received.size(), 2);
    }

    void clientGone_removesRoutes()
    {
        Router r;
        MockSink sink;
        sink.hookRouter(&r);

        r.handleRegister(1, "Ghost");
        r.handleSubscribe(1, "evt");
        r.handleClientGone(1);

        QVERIFY(!r.isRegistered("Ghost"));
        r.handlePublish("evt", "Pub", QJsonObject{{"gone", true}});
        // queued (no subscribers), not delivered
        QCOMPARE(sink.received.size(), 0);
    }

    void pushMessageFormat()
    {
        Router r;
        MockSink sink;
        sink.hookRouter(&r);

        r.handleRegister(1, "Reader");
        r.handleSubscribe(1, "news");
        r.handlePublish("news", "Writer", QJsonObject{{"headline", "ok"}});

        QCOMPARE(sink.received.size(), 1);
        QJsonObject pushed = QJsonDocument::fromJson(sink.received.first().json).object();
        QCOMPARE(pushed["push"].toBool(), true);
        QCOMPARE(pushed["from"].toString(), QString("news"));
        QCOMPARE(pushed["sender"].toString(), QString("Writer"));
        QCOMPARE(pushed["data"].toObject()["headline"].toString(), QString("ok"));
    }

    void publishedSignalEmitted()
    {
        Router r;
        QString lastTag;
        connect(&r, &Router::published, this, [&](const QString &tag, const QString &, const QJsonObject &){
            lastTag = tag;
        });

        r.handleRegister(1, "A");
        r.handleSubscribe(1, "ev");
        r.handlePublish("ev:A", "X", QJsonObject{});
        QCOMPARE(lastTag, QString("ev"));
    }
};

QTEST_MAIN(TstRouter)
#include "tst_router.moc"
