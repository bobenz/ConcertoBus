#include "BusConfigLoader.h"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QUrl>

BusConfigLoader::BusConfigLoader(QObject *parent) : QObject(parent) {}

BusConfig *BusConfigLoader::load(const QString &qmlFilePath)
{
    m_error.clear();

    QQmlEngine engine;
    engine.setOutputWarningsToStandardError(false);

    // Register config types under ConcertoBusConfig 1.0
    qmlRegisterType<BusConfig>    ("ConcertoBusConfig", 1, 0, "BusConfig");
    qmlRegisterType<ProcessDef>   ("ConcertoBusConfig", 1, 0, "Process");
    qmlRegisterType<TransportDef> ("ConcertoBusConfig", 1, 0, "Transport");
    qmlRegisterType<GatewayDef>   ("ConcertoBusConfig", 1, 0, "Gateway");

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlFilePath));
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

    // Transfer ownership away from the QML engine
    QQmlEngine::setObjectOwnership(config, QQmlEngine::CppOwnership);
    config->setParent(nullptr);
    return config;
}
