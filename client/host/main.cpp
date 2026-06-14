#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QtQml>

#include "../AbstractBusClient.h"
#include "../BusClient.h"
#include "../LaunchSpec.h"
#include "../StdioBusClient.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    if (argc < 2) {
        qCritical("Usage: ConcertoBusHost <path/to/Launch.qml>");
        return 1;
    }

    const QFileInfo launchInfo(QString::fromLocal8Bit(argv[1]));
    if (!launchInfo.exists()) {
        qCritical("Launch.qml not found: %s", argv[1]);
        return 1;
    }
    const QString launchDir = launchInfo.absoluteDir().absolutePath();

    // Phase 1 — load LaunchSpec from Launch.qml
    QQmlEngine specEngine;
    qmlRegisterType<LaunchSpec>("ConcertoBus", 1, 0, "LaunchSpec");

    QQmlComponent specComp(&specEngine, QUrl::fromLocalFile(launchInfo.absoluteFilePath()));
    if (specComp.isError()) {
        for (const QQmlError &e : specComp.errors())
            qCritical() << e.toString();
        return 1;
    }

    QScopedPointer<QObject> specObj(specComp.create());
    LaunchSpec *spec = qobject_cast<LaunchSpec *>(specObj.get());
    if (!spec) {
        qCritical("Root of Launch.qml must be a LaunchSpec element");
        return 1;
    }

    // Phase 2 — create the transport-appropriate client
    AbstractBusClient *client =
        (spec->transport() == QLatin1String("tcp"))
            ? static_cast<AbstractBusClient *>(new BusClient(&app))
            : static_cast<AbstractBusClient *>(new StdioBusClient(&app));

    client->setName(spec->name());

    // Phase 3 — main app engine
    QQmlApplicationEngine engine;

    for (const QString &rel : spec->importPaths())
        engine.addImportPath(QDir(launchDir).absoluteFilePath(rel));

    engine.rootContext()->setContextProperty(QStringLiteral("busClient"), client);

    client->connectToBus();
    for (const QString &tag : spec->subscribes())
        client->subscribe(tag);

    const QString mainPath = QDir(launchDir).absoluteFilePath(spec->mainQml());
    engine.load(QUrl::fromLocalFile(mainPath));
    if (engine.rootObjects().isEmpty())
        return 1;

    spec->emitCompleted();

    return app.exec();
}
