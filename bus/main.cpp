#include "BusCore.h"
#include "ProcessManager.h"
#include "config/BusConfigLoader.h"
#include "config/BusConfigTypes.h"
#include <QCoreApplication>
#include <QCommandLineParser>
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

    ProcessManager pm;
    pm.load(config);

    BusCore core;
    core.setProcessManager(&pm);

    // autoRestart re-attach: when PM relaunches a crashed stdio process,
    // re-attach the new QProcess to StdioTransport so routing continues.
    QObject::connect(&pm, &ProcessManager::processStarted, &core,
                     [&pm, &core](const QString &name) {
        if (pm.transportFor(name) == QLatin1String("stdio")) {
            if (QProcess *proc = pm.processFor(name))
                core.attachProcess(proc, pm.subscriptionsFor(name));
        }
    });

    qInfo() << "ConcertoBusDaemon started. Defined processes:" << pm.names();
    return app.exec();
}
