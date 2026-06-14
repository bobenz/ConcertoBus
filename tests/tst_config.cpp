#include <QtTest>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "../bus/config/BusConfigLoader.h"
#include "../bus/config/BusConfigTypes.h"

// Writes a file relative to dir, returns full path.
static bool writeFile(const QString &dir, const QString &name, const QString &content)
{
    QFile f(QDir(dir).filePath(name));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream(&f) << content;
    return true;
}

class TstConfig : public QObject
{
    Q_OBJECT

private slots:
    void test_basicLoad()
    {
        // Create two app directories, each with a Launch.qml
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString app1Dir = QDir(tmpDir.path()).filePath("App1");
        const QString app2Dir = QDir(tmpDir.path()).filePath("App2");
        QVERIFY(QDir().mkpath(app1Dir));
        QVERIFY(QDir().mkpath(app2Dir));

        QVERIFY(writeFile(app1Dir, "Launch.qml",
            "import ConcertoBus 1.0\n"
            "LaunchSpec { name: \"App1\"; transport: \"stdio\"; subscribes: [\"sensor\", \"control\"] }\n"
        ));
        QVERIFY(writeFile(app2Dir, "Launch.qml",
            "import ConcertoBus 1.0\n"
            "LaunchSpec { name: \"App2\"; transport: \"tcp\"; subscribes: [\"display\"] }\n"
        ));

        const QString configQml =
            "import ConcertoBusConfig 1.0\n"
            "BusConfig {\n"
            "    transports: [ Transport { plugin: \"tcp\"; port: 49152 } ]\n"
            "    App { path: \"App1\" }\n"
            "    App { path: \"App2\" }\n"
            "}\n";

        const QString configPath = QDir(tmpDir.path()).filePath("config.qml");
        QVERIFY(writeFile(tmpDir.path(), "config.qml", configQml));

        BusConfigLoader loader;
        BusConfig *cfg = loader.load(configPath);
        QVERIFY2(cfg, qPrintable(loader.errorString()));

        QCOMPARE(cfg->processList().size(), 2);
        QCOMPARE(cfg->transportList().size(), 1);
        QCOMPARE(cfg->gatewayList().size(), 0);

        // Order matches config order
        ProcessDef *p0 = cfg->processList().at(0);
        QCOMPARE(p0->name(), QStringLiteral("App1"));
        QCOMPARE(p0->subscribes(), QStringList({"sensor", "control"}));
        QCOMPARE(p0->transport(), QStringLiteral("stdio"));

        ProcessDef *p1 = cfg->processList().at(1);
        QCOMPARE(p1->name(), QStringLiteral("App2"));
        QCOMPARE(p1->transport(), QStringLiteral("tcp"));
        QCOMPARE(p1->subscribes(), QStringList({"display"}));

        TransportDef *t0 = cfg->transportList().at(0);
        QCOMPARE(t0->plugin(), QStringLiteral("tcp"));
        QCOMPARE(t0->port(), 49152);

        delete cfg;
    }

    void test_autoLaunch()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString appDir = QDir(tmpDir.path()).filePath("MyApp");
        QVERIFY(QDir().mkpath(appDir));
        QVERIFY(writeFile(appDir, "Launch.qml",
            "import ConcertoBus 1.0\n"
            "LaunchSpec { name: \"MyApp\"; transport: \"stdio\" }\n"
        ));

        const QString configQml =
            "import QtQml\n"
            "import ConcertoBusConfig 1.0\n"
            "BusConfig {\n"
            "    App { id: app; path: \"MyApp\" }\n"
            "    Component.onCompleted: { app.start() }\n"
            "}\n";
        QVERIFY(writeFile(tmpDir.path(), "config.qml", configQml));

        BusConfigLoader loader;
        BusConfig *cfg = loader.load(QDir(tmpDir.path()).filePath("config.qml"));
        QVERIFY2(cfg, qPrintable(loader.errorString()));

        QCOMPARE(cfg->processList().size(), 1);
        QVERIFY(cfg->processList().at(0)->autoLaunch());

        delete cfg;
    }

    void test_emptyConfig()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QVERIFY(writeFile(tmpDir.path(), "config.qml",
            "import ConcertoBusConfig 1.0\nBusConfig {}\n"));

        BusConfigLoader loader;
        BusConfig *cfg = loader.load(QDir(tmpDir.path()).filePath("config.qml"));
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
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QVERIFY(writeFile(tmpDir.path(), "config.qml",
            "import QtQml 2.0\nQtObject {}\n"));

        BusConfigLoader loader;
        BusConfig *cfg = loader.load(QDir(tmpDir.path()).filePath("config.qml"));
        QVERIFY(cfg == nullptr);
    }
};

QTEST_MAIN(TstConfig)
#include "tst_config.moc"
