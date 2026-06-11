#include "ProcessManager.h"
#include "config/BusConfigTypes.h"

#include <QProcess>
#include <QTimer>

static const int kStableUptimeMs = 10'000;

ProcessManager::ProcessManager(QObject *parent) : QObject(parent) {}

ProcessManager::~ProcessManager()
{
    m_destroying = true;
    for (auto &e : m_entries) {
        if (e.process) {
            e.process->disconnect(); // prevent finished/errorOccurred from calling back
            e.process->kill();
            e.process->waitForFinished(500);
        }
    }
}

bool ProcessManager::load(BusConfig *config)
{
    if (!config)
        return false;

    m_entries.clear();

    for (ProcessDef *def : config->processList()) {
        if (def->name().isEmpty() || def->exe().isEmpty())
            continue;

        Entry e;
        e.exe          = def->exe();
        e.args         = def->args();
        e.workingDir   = def->workingDir();
        e.subscribes   = def->subscribes();
        e.transport    = def->transport();
        e.autoRestart  = def->autoRestart();
        e.restartDelayMs = def->restartDelay();
        e.maxRestarts  = def->maxRestarts();
        m_entries.insert(def->name(), e);
    }

    return true;
}

void ProcessManager::addEntry(const QString &name, const QString &exe,
                              const QStringList &args,
                              const QString &workingDir,
                              const QStringList &subscribes,
                              const QString &transport,
                              bool autoRestart,
                              int restartDelayMs,
                              int maxRestarts)
{
    Entry e;
    e.exe           = exe;
    e.args          = args;
    e.workingDir    = workingDir;
    e.subscribes    = subscribes;
    e.transport     = transport.isEmpty() ? QStringLiteral("stdio") : transport;
    e.autoRestart   = autoRestart;
    e.restartDelayMs = restartDelayMs;
    e.maxRestarts   = maxRestarts;
    m_entries.insert(name, e);
}

bool ProcessManager::launch(const QString &name)
{
    if (!m_entries.contains(name))
        return false;

    Entry &e = m_entries[name];
    if (e.process && e.process->state() != QProcess::NotRunning)
        return true; // already running

    delete e.process;
    e.process = new QProcess(this);

    if (!e.workingDir.isEmpty())
        e.process->setWorkingDirectory(e.workingDir);

    // Reset restart counter once the process stays alive for kStableUptimeMs
    QTimer *stableTimer = new QTimer(e.process);
    stableTimer->setSingleShot(true);
    stableTimer->setInterval(kStableUptimeMs);

    connect(e.process, &QProcess::started, stableTimer, qOverload<>(&QTimer::start));
    connect(stableTimer, &QTimer::timeout, this, [this, name]() {
        if (m_entries.contains(name))
            m_entries[name].restartCount = 0;
    });

    connect(e.process, &QProcess::started, this, [this, name]() {
        emit processStarted(name);
    });

    connect(e.process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, name](int code, QProcess::ExitStatus status) {
        onProcessFinished(name, code, static_cast<int>(status));
    });

    connect(e.process, &QProcess::errorOccurred, this, [this, name](QProcess::ProcessError err) {
        if (err != QProcess::Crashed) // Crashed is also reported via finished()
            onProcessFinished(name, -1, static_cast<int>(QProcess::CrashExit));
    });

    e.process->start(e.exe, e.args);
    return e.process->waitForStarted(3000) || e.process->state() == QProcess::Starting;
}

void ProcessManager::kill(const QString &name)
{
    if (!m_entries.contains(name))
        return;
    Entry &e = m_entries[name];
    if (e.process && e.process->state() != QProcess::NotRunning) {
        e.autoRestart = false; // prevent auto-restart on kill
        e.process->kill();
    }
}

void ProcessManager::restart(const QString &name)
{
    kill(name);
    // kill() is async — wait for finished, then re-launch
    if (m_entries.contains(name) && m_entries[name].process) {
        connect(m_entries[name].process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [this, name]() {
            if (m_destroying || !m_entries.contains(name))
                return;
            m_entries[name].autoRestart = true;
            launch(name);
            emit processRestarted(name);
        }, Qt::SingleShotConnection);
        m_entries[name].autoRestart = false; // suppress normal autoRestart until re-launch
    }
}

bool ProcessManager::isRunning(const QString &name) const
{
    auto it = m_entries.find(name);
    return it != m_entries.end() && it->process
           && it->process->state() == QProcess::Running;
}

QStringList ProcessManager::names() const
{
    return m_entries.keys();
}

QStringList ProcessManager::subscriptionsFor(const QString &name) const
{
    return m_entries.value(name).subscribes;
}

QString ProcessManager::transportFor(const QString &name) const
{
    return m_entries.value(name).transport;
}

void ProcessManager::onProcessFinished(const QString &name, int /*exitCode*/, int exitStatus)
{
    if (!m_entries.contains(name))
        return;

    Entry &e = m_entries[name];
    bool crashed = (exitStatus == static_cast<int>(QProcess::CrashExit));

    if (crashed)
        emit processCrashed(name);
    else
        emit processStopped(name);

    if (e.autoRestart && e.restartCount < e.maxRestarts) {
        scheduleRestart(name);
    }
}

void ProcessManager::scheduleRestart(const QString &name)
{
    Entry &e = m_entries[name];
    e.restartCount++;
    int delay = e.restartDelayMs;

    QTimer::singleShot(delay, this, [this, name]() {
        if (m_destroying || !m_entries.contains(name))
            return;
        launch(name);
    });
}
