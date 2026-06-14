#include "BusCore.h"
#include "ProcessManager.h"
#include "config/BusConfigLoader.h"
#include "config/BusConfigTypes.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("ConcertoBusDaemon");
    app.setApplicationVersion("2.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"c","config"}, "Path to bus config QML file", "config"});
    parser.addOption({{"l","launch"}, "Launch a named process on startup (repeatable)", "name"});
    parser.process(app);

    const QString configPath = parser.value("config");
    if (configPath.isEmpty()) {
        qCritical() << "No config file specified. Use -c <path>";
        return 1;
    }

    BusConfigLoader loader;
    BusConfig *config = loader.load(configPath);
    if (!config) {
        qCritical() << "Failed to load config:" << configPath;
        return 1;
    }

    const QString configDir = QFileInfo(configPath).absoluteDir().absolutePath();

    ProcessManager pm;
    pm.load(config, configDir);

    BusCore core;
    core.setProcessManager(&pm);

    // When a stdio process (re)starts, attach it to the bus.
    QObject::connect(&pm, &ProcessManager::processStarted, &core,
                     [&pm, &core](const QString &name) {
        if (pm.transportFor(name) == QLatin1String("stdio")) {
            if (QProcess *proc = pm.processFor(name))
                core.attachProcess(proc, pm.subscriptionsFor(name));
        }
    });

    qInfo() << "ConcertoBusDaemon started. Defined processes:" << pm.names();

    // --launch <name>: auto-launch named processes from the command line.
    for (const QString &name : parser.values("launch")) {
        qInfo() << "Auto-launching:" << name;
        if (!pm.launch(name))
            qWarning() << "Failed to launch:" << name;
    }

    return app.exec();
}
