#include "StdioTransport.h"
#include <QProcess>

StdioTransport::StdioTransport(QObject *parent) : IBusTransport(parent) {}

bool StdioTransport::start(const QVariantMap &)
{
    return true;
}

void StdioTransport::addProcess(QProcess *proc)
{
    if (m_procToId.contains(proc))
        return;

    ClientId id = m_nextId++;
    m_procToId.insert(proc, id);
    m_idToProc.insert(id, proc);
    m_readBuf.insert(id, {});

    connect(proc, &QProcess::readyReadStandardOutput, this, &StdioTransport::onReadyRead);
    connect(proc, &QProcess::finished,  this, &StdioTransport::onProcessFinished);

    emit clientConnected(id);
}

void StdioTransport::removeProcess(QProcess *proc)
{
    auto it = m_procToId.find(proc);
    if (it == m_procToId.end()) return;
    ClientId id = it.value();

    proc->disconnect(this);
    m_procToId.erase(it);
    m_idToProc.remove(id);
    m_readBuf.remove(id);
    // No clientDisconnected — caller handles it (process already dead).
}

void StdioTransport::send(ClientId id, const QByteArray &json)
{
    QProcess *proc = m_idToProc.value(id, nullptr);
    if (!proc || proc->state() != QProcess::Running) return;

    QByteArray line = json;
    if (!line.endsWith('\n')) line += '\n';
    proc->write(line);
}

void StdioTransport::closeClient(ClientId id)
{
    QProcess *proc = m_idToProc.value(id, nullptr);
    if (!proc) return;
    proc->terminate();
}

void StdioTransport::onReadyRead()
{
    auto *proc = qobject_cast<QProcess *>(sender());
    if (!proc) return;

    ClientId id = idFor(proc);
    if (!id) return;

    m_readBuf[id] += proc->readAllStandardOutput();
    QByteArray &buf = m_readBuf[id];

    int nl;
    while ((nl = buf.indexOf('\n')) >= 0) {
        QByteArray line = buf.left(nl).trimmed();
        buf.remove(0, nl + 1);
        if (!line.isEmpty())
            emit messageReceived(id, line);
    }
}

void StdioTransport::onProcessFinished(int, QProcess::ExitStatus)
{
    auto *proc = qobject_cast<QProcess *>(sender());
    if (!proc) return;

    auto it = m_procToId.find(proc);
    if (it == m_procToId.end()) return;
    ClientId id = it.value();

    proc->disconnect(this);
    m_procToId.erase(it);
    m_idToProc.remove(id);
    m_readBuf.remove(id);

    emit clientDisconnected(id);
}

ClientId StdioTransport::idFor(QProcess *proc) const
{
    return m_procToId.value(proc, 0);
}
