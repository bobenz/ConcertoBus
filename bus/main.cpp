#include "BusCore.h"
#include "IBusTransport.h"
#include "IBusGateway.h"
#include "ProcessManager.h"
#include "config/BusConfigLoader.h"
#include "config/BusConfigTypes.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QDebug>

static IBusGateway *loadGateway(const QString &gatewaysDir, const QString &key)
{
    const QDir dir(gatewaysDir);
    const QString name = dir.absoluteFilePath(
        key.at(0).toUpper() + key.mid(1) + QStringLiteral("Gateway"));
    QPluginLoader loader(name);
    QObject *obj = loader.instance();
    if (!obj) {
        qCritical() << "Cannot load gateway plugin" << key << ":" << loader.errorString();
        return nullptr;
    }
    auto *gw = qobject_cast<IBusGateway *>(obj);
    if (!gw) {
        qCritical() << "Plugin" << loader.fileName() << "does not implement IBusGateway";
        return nullptr;
    }
    qInfo() << "Loaded gateway plugin:" << loader.fileName();
    return gw;
}

static IBusTransport *loadTransport(const QString &pluginsDir, const QString &key)
{
    const QDir dir(pluginsDir);
    // Try <key>.dll / lib<key>.so etc. — Qt normalises the name.
    const QString name = dir.absoluteFilePath(key);
    QPluginLoader loader(name);
    QObject *obj = loader.instance();
    if (!obj) {
        // Retry with capitalised name (TcpTransport.dll)
        const QString nameU = dir.absoluteFilePath(
            key.at(0).toUpper() + key.mid(1) + QStringLiteral("Transport"));
        loader.setFileName(nameU);
        obj = loader.instance();
    }
    if (!obj) {
        qCritical() << "Cannot load transport plugin" << key << ":" << loader.errorString();
        return nullptr;
    }
    auto *transport = qobject_cast<IBusTransport *>(obj);
    if (!transport) {
        qCritical() << "Plugin" << loader.fileName() << "does not implement IBusTransport";
        return nullptr;
    }
    qInfo() << "Loaded transport plugin:" << loader.fileName();
    return transport;
}

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
        qCritical() << "Failed to load config:" << configPath << "-" << loader.errorString();
        return 1;
    }

    const QString configDir = QFileInfo(configPath).absoluteDir().absolutePath();

    ProcessManager pm;
    pm.load(config, configDir);

    // Transport plugins live in plugins/, gateway plugins in gateways/.
    const QString appDir     = QCoreApplication::applicationDirPath();
    const QString pluginsDir = QDir(appDir).absoluteFilePath("plugins");
    const QString gatewaysDir = QDir(appDir).absoluteFilePath("gateways");

    BusCore core;
    core.setProcessManager(&pm);

    // Load transport plugins declared in config.
    for (TransportDef *td : config->transportList()) {
        const QString key = td->plugin().toLower();
        if (key == QLatin1String("stdio"))
            continue;  // stdio is built-in, handled by StdioTransport inside BusCore
        IBusTransport *factory = loadTransport(pluginsDir, key);
        if (!factory) return 1;
        IBusTransport *t = factory->createInstance(&app);
        t->start(td->options());
        core.addTransport(t);
    }

    // Load gateway plugins declared in config.
    for (GatewayDef *gd : config->gatewayList()) {
        IBusGateway *gw = loadGateway(gatewaysDir, gd->plugin().toLower());
        if (!gw) return 1;
        gw->start(gd->options());
        core.addGateway(gw);
    }

    // When a stdio process (re)starts, attach it to the bus.
    QObject::connect(&pm, &ProcessManager::processStarted, &core,
                     [&pm, &core](const QString &name) {
        if (pm.transportFor(name) == QLatin1String("stdio")) {
            if (QProcess *proc = pm.processFor(name))
                core.attachProcess(proc, pm.subscriptionsFor(name));
        }
    });

    qInfo() << "ConcertoBusDaemon started. Defined processes:" << pm.names();

    for (const QString &name : pm.autoLaunchNames()) {
        qInfo() << "Auto-launching:" << name;
        if (!pm.launch(name))
            qWarning() << "Failed to launch:" << name;
    }

    for (const QString &name : parser.values("launch")) {
        qInfo() << "Auto-launching (from CLI):" << name;
        if (!pm.launch(name))
            qWarning() << "Failed to launch:" << name;
    }

    return app.exec();
}
