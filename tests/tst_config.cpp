#include <QtTest>
#include <QDir>
#include <QTemporaryFile>
#include <QTextStream>

#include "../bus/config/BusConfigLoader.h"
#include "../bus/config/BusConfigTypes.h"

class TstConfig : public QObject
{
    Q_OBJECT

private:
    QString writeConfig(const QString &content) {
        // Write to a temp .qml file and return the path
        auto *f = new QTemporaryFile(QDir::tempPath() + "/tst_config_XXXXXX.qml", this);
        f->setAutoRemove(true);
        if (!f->open()) return {};
        QTextStream(f) << content;
        f->close();
        return f->fileName();
    }

private slots:
    void test_basicLoad()
    {
        const QString qml = R"(
import ConcertoBusConfig 1.0
BusConfig {
    transports: [ Transport { plugin: "tcp"; port: 49152 } ]
    Process {
        name: "App1"
        exe: "C:/test/app1.exe"
        subscribes: ["sensor", "control"]
        autoRestart: true
        restartDelay: 2000
        maxRestarts: 4
    }
    Process {
        name: "App2"
        exe: "C:/test/app2.exe"
        transport: "tcp"
        subscribes: ["display"]
    }
})";

        BusConfigLoader loader;
        BusConfig *cfg = loader.load(writeConfig(qml));
        QVERIFY2(cfg, qPrintable(loader.errorString()));

        QCOMPARE(cfg->processList().size(), 2);
        QCOMPARE(cfg->transportList().size(), 1);
        QCOMPARE(cfg->gatewayList().size(), 0);

        ProcessDef *p0 = cfg->processList().at(0);
        QCOMPARE(p0->name(), QStringLiteral("App1"));
        QCOMPARE(p0->exe(), QStringLiteral("C:/test/app1.exe"));
        QCOMPARE(p0->subscribes(), QStringList({"sensor", "control"}));
        QVERIFY(p0->autoRestart());
        QCOMPARE(p0->restartDelay(), 2000);
        QCOMPARE(p0->maxRestarts(), 4);
        QCOMPARE(p0->transport(), QStringLiteral("stdio")); // default

        ProcessDef *p1 = cfg->processList().at(1);
        QCOMPARE(p1->name(), QStringLiteral("App2"));
        QCOMPARE(p1->transport(), QStringLiteral("tcp"));
        QVERIFY(!p1->autoRestart());

        TransportDef *t0 = cfg->transportList().at(0);
        QCOMPARE(t0->plugin(), QStringLiteral("tcp"));
        QCOMPARE(t0->port(), 49152);

        delete cfg;
    }

    void test_emptyConfig()
    {
        const QString qml = "import ConcertoBusConfig 1.0\nBusConfig {}\n";
        BusConfigLoader loader;
        BusConfig *cfg = loader.load(writeConfig(qml));
        QVERIFY2(cfg, qPrintable(loader.errorString()));
        QCOMPARE(cfg->processList().size(), 0);
        delete cfg;
    }

    void test_badFile()
    {
        BusConfigLoader loader;
        BusConfig *cfg = loader.load(QStringLiteral("/nonexistent/path/config.qml"));
        QVERIFY(cfg == nullptr);
        QVERIFY(!loader.errorString().isEmpty());
    }

    void test_wrongRootType()
    {
        const QString qml = "import QtQml 2.0\nQtObject {}\n";
        BusConfigLoader loader;
        BusConfig *cfg = loader.load(writeConfig(qml));
        QVERIFY(cfg == nullptr);
    }
};

QTEST_MAIN(TstConfig)
#include "tst_config.moc"
