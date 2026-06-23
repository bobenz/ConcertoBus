#pragma once
#include "Router.h"
#include "IBusTransport.h"
#include "IBusGateway.h"
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>

class StdioTransport;
class ProcessManager;
class QProcess;

// Wires one or more IBusTransport instances to the Router.
// Handles the mandatory registration handshake and routes protocol commands.
class BusCore : public QObject
{
    Q_OBJECT
public:
    explicit BusCore(QObject *parent = nullptr);

    // Add a transport.  BusCore takes ownership if reparent is true (default).
    void addTransport(IBusTransport *transport, bool reparent = true);

    // Convenience: access the built-in StdioTransport (created in constructor).
    StdioTransport *stdioTransport() const;

    // Tell BusCore about a process that was launched by ProcessManager.
    // BusCore hooks it to StdioTransport and waits for the registration handshake.
    void attachProcess(QProcess *proc, const QStringList &autoSubscribeTags = {});

    // Wire ProcessManager so that launch/kill/restart commands are forwarded to it
    // and PM lifecycle signals are pushed to watching clients.
    void setProcessManager(ProcessManager *pm);

    // Add a federation gateway. BusCore connects Router::published to the gateway
    // and feeds gateway::remotePublished back into the Router.
    void addGateway(IBusGateway *gw);

    Router *router() const { return m_router; }

signals:
    void clientRegistered(ClientId id, const QString &name);
    void clientGone(ClientId id, const QString &name);

private slots:
    void onClientDisconnected(ClientId id);
    void onMessageReceived(ClientId id, const QByteArray &json);
    void onPmStarted(const QString &name);
    void onPmStopped(const QString &name);
    void onPmCrashed(const QString &name);

private:
    void dispatchCommand(ClientId id, const QJsonObject &cmd);
    void sendJson(ClientId id, const QJsonObject &obj);
    void notifyWatchers(const QString &processName, const QJsonObject &msg);

    Router          *m_router;
    StdioTransport  *m_stdio;
    ProcessManager  *m_pm = nullptr;

    // transport that owns each client
    QHash<ClientId, IBusTransport *> m_clientTransport;
    QList<IBusGateway *> m_gateways;

    // clients not yet registered: id → auto-subscribe tags queued from attachProcess
    QHash<ClientId, QStringList> m_pendingAutoSubs;

    // processName → clients watching lifecycle events
    QHash<QString, QList<ClientId>> m_processWatchers;
};
