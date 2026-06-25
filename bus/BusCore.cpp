#include "BusCore.h"
#include "ProcessManager.h"
#include "StdioTransport.h"
#include <QJsonDocument>
#include <QJsonArray>

BusCore::BusCore(QObject *parent)
    : QObject(parent)
    , m_router(new Router(this))
    , m_stdio(new StdioTransport(this))
{
    qRegisterMetaType<ClientId>("ClientId");
    addTransport(m_stdio, false);
    connect(m_router, &Router::sendToClient, this, [this](ClientId id, const QByteArray &json) {
        auto *transport = m_clientTransport.value(id, nullptr);
        if (transport) transport->send(id, json);
    });
}

void BusCore::addTransport(IBusTransport *transport, bool reparent)
{
    if (reparent) transport->setParent(this);

    connect(transport, &IBusTransport::clientConnected, this,
            [this, transport](ClientId id) {
                m_clientTransport.insert(id, transport);
            });
    connect(transport, &IBusTransport::clientDisconnected,
            this, &BusCore::onClientDisconnected);
    connect(transport, &IBusTransport::messageReceived,
            this, &BusCore::onMessageReceived);
}

StdioTransport *BusCore::stdioTransport() const { return m_stdio; }

void BusCore::attachProcess(QProcess *proc, const QStringList &autoSubscribeTags)
{
    // We don't know the ClientId yet — it comes from addProcess → clientConnected.
    // Stash the tags keyed by process pointer; onClientConnected will look them up.
    // Use a temporary connection to capture the id for this specific process.
    // Temporary one-shot connection: capture the ClientId when this process
    // connects, then disconnect. Using shared_ptr for Qt 5/6 compatibility
    // (Qt::SingleShotConnection requires Qt 6).
    auto connPtr = std::make_shared<QMetaObject::Connection>();
    *connPtr = connect(m_stdio, &IBusTransport::clientConnected, this,
            [this, proc, autoSubscribeTags, connPtr](ClientId id) {
                QObject::disconnect(*connPtr);
                if (m_pendingAutoSubs.contains(id))
                    return; // already handled
                m_pendingAutoSubs.insert(id, autoSubscribeTags);
                Q_UNUSED(proc)
            });

    m_stdio->addProcess(proc);
}

void BusCore::addGateway(IBusGateway *gw)
{
    m_gateways.append(gw);
    connect(m_router, &Router::published, gw, &IBusGateway::onLocalPublish);
    connect(gw, &IBusGateway::remotePublished, this,
            [this](const QString &tag, const QString &sender, const QJsonObject &data) {
                m_router->handlePublish(tag, sender, data);
            });
    connect(gw, &IBusGateway::remoteEvent, this,
            [this](const QString &event, const QString &name) {
                notifyWatchers(name, QJsonObject{
                    {QStringLiteral("event"), event},
                    {QStringLiteral("name"), name}});
            });
    connect(gw, &IBusGateway::remoteCommand, this,
            [this](const QString &cmd, const QString &name) {
                if (!m_pm) return;
                if (cmd == QLatin1String("launch"))        m_pm->launch(name);
                else if (cmd == QLatin1String("kill"))     m_pm->kill(name);
                else if (cmd == QLatin1String("restart"))  m_pm->restart(name);
            });
}

void BusCore::setProcessManager(ProcessManager *pm)
{
    m_pm = pm;
    connect(pm, &ProcessManager::processStarted, this, &BusCore::onPmStarted);
    connect(pm, &ProcessManager::processStopped, this, &BusCore::onPmStopped);
    connect(pm, &ProcessManager::processCrashed, this, &BusCore::onPmCrashed);
}

void BusCore::onClientDisconnected(ClientId id)
{
    const QString name = m_router->nameOf(id);
    m_router->handleClientGone(id);
    m_clientTransport.remove(id);
    m_pendingAutoSubs.remove(id);
    // Remove this client from all watcher lists
    for (auto &watchers : m_processWatchers)
        watchers.removeAll(id);
    if (!name.isEmpty()) {
        for (IBusGateway *gw : m_gateways) gw->onLocalUnregister(name);
        emit clientGone(id, name);
    }
}

void BusCore::onMessageReceived(ClientId id, const QByteArray &json)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("bad_json")}});
        return;
    }
    dispatchCommand(id, doc.object());
}

void BusCore::dispatchCommand(ClientId id, const QJsonObject &cmd)
{
    const QString c = cmd[QStringLiteral("cmd")].toString();

    if (c == QLatin1String("register")) {
        const QString name = cmd[QStringLiteral("name")].toString();
        if (name.isEmpty()) {
            sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("missing_name")}});
            return;
        }
        if (!m_router->handleRegister(id, name)) {
            sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("name_taken")}});
            return;
        }
        sendJson(id, QJsonObject{{QStringLiteral("ok"), true}});
        // Apply auto-subscribe tags queued by attachProcess
        const QStringList tags = m_pendingAutoSubs.take(id);
        for (const QString &tag : tags) {
            m_router->handleSubscribe(id, tag);
            for (IBusGateway *gw : m_gateways) gw->onLocalSubscribe(tag);
        }
        for (IBusGateway *gw : m_gateways) gw->onLocalRegister(name);
        emit clientRegistered(id, name);
        // Notify any clients waiting for this process to come up
        if (m_processWatchers.contains(name)) {
            notifyWatchers(name, QJsonObject{
                {QStringLiteral("event"), QStringLiteral("process_started")},
                {QStringLiteral("name"), name}});
        }
        return;
    }

    // All other commands require prior registration
    if (m_router->nameOf(id).isEmpty()) {
        sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("not_registered")}});
        return;
    }

    const QString sender = m_router->nameOf(id);

    if (c == QLatin1String("subscribe")) {
        const QString tag = cmd[QStringLiteral("tag")].toString();
        if (!tag.isEmpty()) {
            m_router->handleSubscribe(id, tag);
            for (IBusGateway *gw : m_gateways) gw->onLocalSubscribe(tag);
        }
        sendJson(id, QJsonObject{{QStringLiteral("ok"), true}});
    } else if (c == QLatin1String("unsubscribe")) {
        const QString tag = cmd[QStringLiteral("tag")].toString();
        if (!tag.isEmpty()) {
            m_router->handleUnsubscribe(id, tag);
            for (IBusGateway *gw : m_gateways) gw->onLocalUnsubscribe(tag);
        }
        sendJson(id, QJsonObject{{QStringLiteral("ok"), true}});
    } else if (c == QLatin1String("publish")) {
        const QString to   = cmd[QStringLiteral("to")].toString();
        const QJsonObject data = cmd[QStringLiteral("data")].toObject();
        if (!to.isEmpty()) m_router->handlePublish(to, sender, data);
    } else if (c == QLatin1String("launch") || c == QLatin1String("kill") || c == QLatin1String("restart")) {
        const QString name = cmd[QStringLiteral("name")].toString();
        if (name.isEmpty() || !m_pm) {
            sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("unknown_process")}});
            return;
        }
        if (!m_pm->names().contains(name)) {
            sendJson(id, QJsonObject{
                {QStringLiteral("error"), QStringLiteral("unknown_process")},
                {QStringLiteral("name"), name}});
            return;
        }
        // Register caller as a watcher for this process
        auto &watchers = m_processWatchers[name];
        if (!watchers.contains(id)) watchers.append(id);

        if (c == QLatin1String("launch")) {
            if (m_router->isRegistered(name)) {
                sendJson(id, QJsonObject{
                    {QStringLiteral("event"), QStringLiteral("process_started")},
                    {QStringLiteral("name"), name}});
            } else {
                sendJson(id, QJsonObject{
                    {QStringLiteral("event"), QStringLiteral("launching")},
                    {QStringLiteral("name"), name}});
                if (!m_pm->isRunning(name)) {
                    m_pm->launch(name);
                    if (m_pm->transportFor(name) == QLatin1String("stdio")) {
                        if (QProcess *proc = m_pm->processFor(name))
                            attachProcess(proc, m_pm->subscriptionsFor(name));
                    }
                }
            }
        } else if (c == QLatin1String("kill")) {
            m_pm->kill(name);
        } else {
            m_pm->restart(name);
        }
    } else {
        sendJson(id, QJsonObject{{QStringLiteral("error"), QStringLiteral("unknown_cmd")}});
    }
}

void BusCore::sendJson(ClientId id, const QJsonObject &obj)
{
    auto *transport = m_clientTransport.value(id, nullptr);
    if (!transport) return;
    QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    transport->send(id, line);
}

void BusCore::notifyWatchers(const QString &processName, const QJsonObject &msg)
{
    const QList<ClientId> watchers = m_processWatchers.value(processName);
    for (ClientId wid : watchers)
        sendJson(wid, msg);
}

void BusCore::onPmStarted(const QString &/*name*/)
{
    // process_started is notified when the process registers on the bus
    // (see dispatchCommand "register" branch) — not here.
}

void BusCore::onPmStopped(const QString &name)
{
    notifyWatchers(name, QJsonObject{
        {QStringLiteral("event"), QStringLiteral("process_stopped")},
        {QStringLiteral("name"), name}});
}

void BusCore::onPmCrashed(const QString &name)
{
    notifyWatchers(name, QJsonObject{
        {QStringLiteral("event"), QStringLiteral("process_crashed")},
        {QStringLiteral("name"), name}});
}
