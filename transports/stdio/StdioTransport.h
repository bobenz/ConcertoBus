#pragma once
#include "IBusTransport.h"
#include <QByteArray>
#include <QHash>
#include <QProcess>

// Built-in transport that manages one or more child processes connected via
// their stdin/stdout.  Each QProcess maps to a unique ClientId.
//
// Typical use: ProcessManager launches a child with QProcess; BusCore calls
// addProcess() to register it with this transport.  The child uses
// StdioBusClient (QML plugin) which speaks the same newline-delimited JSON
// protocol over its own stdin/stdout.
class StdioTransport : public IBusTransport
{
    Q_OBJECT
    Q_INTERFACES(IBusTransport)
public:
    explicit StdioTransport(QObject *parent = nullptr);

    // IBusTransport interface
    bool start(const QVariantMap &config) override;   // no-op for stdio; always returns true
    void send(ClientId id, const QByteArray &json) override;
    void closeClient(ClientId id) override;

    // Called by BusCore after ProcessManager has started a process.
    // Hooks into QProcess readyRead / finished to drive the IBusTransport signals.
    void addProcess(QProcess *proc);

    // Remove a process that is already dead (prevents double-disconnect signals).
    void removeProcess(QProcess *proc);

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    ClientId idFor(QProcess *proc) const;

    QHash<QProcess*, ClientId> m_procToId;
    QHash<ClientId, QProcess*> m_idToProc;
    QHash<ClientId, QByteArray> m_readBuf;  // per-client line buffer
    ClientId m_nextId = 1;
};
