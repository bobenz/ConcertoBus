#pragma once
#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

class QProcess;
class BusConfig;

class ProcessManager : public QObject
{
    Q_OBJECT
public:
    explicit ProcessManager(QObject *parent = nullptr);
    ~ProcessManager();

    bool load(BusConfig *config);

    // Direct C++ entry — useful for tests and manual setup (bypasses QML config)
    void addEntry(const QString &name, const QString &exe,
                  const QStringList &args = {},
                  const QString &workingDir = {},
                  const QStringList &subscribes = {},
                  const QString &transport = QStringLiteral("stdio"),
                  bool autoRestart = false,
                  int restartDelayMs = 1000,
                  int maxRestarts = 5);

    bool launch(const QString &name);
    void kill(const QString &name);
    void restart(const QString &name);

    bool isRunning(const QString &name) const;
    QProcess *processFor(const QString &name) const;
    QStringList names() const;
    QStringList subscriptionsFor(const QString &name) const;
    QString transportFor(const QString &name) const;

signals:
    void processStarted(const QString &name);
    void processStopped(const QString &name);
    void processCrashed(const QString &name);
    void processRestarted(const QString &name);

private:
    bool m_destroying = false;

    struct Entry {
        QString exe;
        QStringList args;
        QString workingDir;
        QStringList subscribes;
        QString transport;          // "stdio" or "tcp"
        bool autoRestart = false;
        int restartDelayMs = 1000;
        int maxRestarts = 5;

        QProcess *process = nullptr;
        int restartCount = 0;
    };

    void onProcessFinished(const QString &name, int exitCode, int exitStatus);
    void scheduleRestart(const QString &name);

    QHash<QString, Entry> m_entries;
};
