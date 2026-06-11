#include "BusConfigLoader.h"
#include "ProcessManager.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>

// TODO Step 8: instantiate BusCore here and wire transports/gateways.
// For now this is a placeholder that loads config and starts the process manager.

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

    return app.exec();
}
