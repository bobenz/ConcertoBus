#include "BusConfigLoader.h"
#include "LaunchSpecReader.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QUrl>

BusConfigLoader::BusConfigLoader(QObject *parent) : QObject(parent) {}

BusConfig *BusConfigLoader::load(const QString &qmlFilePath)
{
    m_error.clear();

    QQmlEngine engine;

    qmlRegisterType<BusConfig>    ("ConcertoBusConfig", 1, 0, "BusConfig");
    qmlRegisterType<AppDef>       ("ConcertoBusConfig", 1, 0, "App");
    qmlRegisterType<TransportDef> ("ConcertoBusConfig", 1, 0, "Transport");
    qmlRegisterType<GatewayDef>   ("ConcertoBusConfig", 1, 0, "Gateway");

    const QString absPath = QFileInfo(qmlFilePath).absoluteFilePath();
    QQmlComponent component(&engine, QUrl::fromLocalFile(absPath));
    if (component.isError()) {
        m_error = component.errorString();
        return nullptr;
    }

    QObject *root = component.create();
    if (!root) {
        m_error = component.errorString();
        return nullptr;
    }

    auto *config = qobject_cast<BusConfig *>(root);
    if (!config) {
        m_error = QStringLiteral("Root object is not a BusConfig");
        delete root;
        return nullptr;
    }

    // Expand each AppDef → read Launch.qml → synthesize ProcessDef.
    const QString configDir = QFileInfo(absPath).absoluteDir().absolutePath();
    const QString clientExe = QDir(QCoreApplication::applicationDirPath())
                                  .absoluteFilePath(QStringLiteral("client.exe"));

    for (AppDef *app : config->appList()) {
        const QString appDir   = QDir(configDir).absoluteFilePath(app->path());
        const QString launchQml = QDir(appDir).absoluteFilePath(QStringLiteral("Launch.qml"));

        if (!QFileInfo::exists(launchQml)) {
            qWarning() << "[BusConfigLoader] Launch.qml not found:" << launchQml;
            continue;
        }

        // Use a separate engine per Launch.qml so registrations don't collide.
        QQmlEngine specEngine;
        qmlRegisterType<LaunchSpecReader>("ConcertoBus", 1, 0, "LaunchSpec");
        QQmlComponent specComp(&specEngine, QUrl::fromLocalFile(launchQml));
        if (specComp.isError()) {
            qWarning() << "[BusConfigLoader] Error loading" << launchQml
                       << ":" << specComp.errorString();
            continue;
        }
        QObject *specObj = specComp.create();
        if (!specObj) {
            qWarning() << "[BusConfigLoader] Failed to create" << launchQml
                       << ":" << specComp.errorString();
            continue;
        }
        auto *spec = qobject_cast<LaunchSpecReader *>(specObj);
        if (!spec) {
            qWarning() << "[BusConfigLoader] Root of" << launchQml << "is not a LaunchSpec";
            delete specObj;
            continue;
        }

        auto *def = new ProcessDef(config);
        def->setName(spec->name());
        def->setTransport(spec->transport());
        def->setSubscribes(spec->subscribes());

        if (!spec->attachTo().isEmpty()) {
            // Inject-style app: no process to spawn; daemon sends inject to target.
            const QString mainQmlUrl = QUrl::fromLocalFile(
                QDir(appDir).absoluteFilePath(spec->mainQml())).toString();
            def->setAttachTo(spec->attachTo());
            def->setMainQmlUrl(mainQmlUrl);
            qInfo() << "[BusConfigLoader] InjectApp:" << spec->name()
                    << "→" << spec->attachTo();
        } else {
            def->setExe(clientExe);
            def->setArgs({launchQml});
            def->setWorkingDir(appDir);
            if (app->autoLaunch())
                def->start();
            qInfo() << "[BusConfigLoader] App:" << spec->name()
                    << "transport:" << spec->transport()
                    << "autoLaunch:" << app->autoLaunch();
        }
        config->addProcess(def);

        delete specObj;
    }

    QQmlEngine::setObjectOwnership(config, QQmlEngine::CppOwnership);
    config->setParent(nullptr);
    return config;
}
