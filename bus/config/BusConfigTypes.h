#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/QQmlListProperty>

class ProcessDef : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name      READ name      WRITE setName      NOTIFY nameChanged)
    Q_PROPERTY(QString exe       READ exe       WRITE setExe       NOTIFY exeChanged)
    Q_PROPERTY(QStringList args  READ args      WRITE setArgs      NOTIFY argsChanged)
    Q_PROPERTY(QString workingDir READ workingDir WRITE setWorkingDir NOTIFY workingDirChanged)
    Q_PROPERTY(QString transport READ transport WRITE setTransport NOTIFY transportChanged)
    Q_PROPERTY(QStringList subscribes READ subscribes WRITE setSubscribes NOTIFY subscribesChanged)
    Q_PROPERTY(bool autoRestart  READ autoRestart  WRITE setAutoRestart  NOTIFY autoRestartChanged)
    Q_PROPERTY(int restartDelay  READ restartDelay  WRITE setRestartDelay  NOTIFY restartDelayChanged)
    Q_PROPERTY(int maxRestarts   READ maxRestarts   WRITE setMaxRestarts   NOTIFY maxRestartsChanged)
public:
    explicit ProcessDef(QObject *parent = nullptr) : QObject(parent) {}

    QString name() const { return m_name; }
    QString exe() const { return m_exe; }
    QStringList args() const { return m_args; }
    QString workingDir() const { return m_workingDir; }
    QString transport() const { return m_transport.isEmpty() ? QStringLiteral("stdio") : m_transport; }
    QStringList subscribes() const { return m_subscribes; }
    bool autoRestart() const { return m_autoRestart; }
    int restartDelay() const { return m_restartDelay; }
    int maxRestarts() const { return m_maxRestarts; }

    void setName(const QString &v)       { if (m_name != v)       { m_name = v;       emit nameChanged(); } }
    void setExe(const QString &v)        { if (m_exe != v)        { m_exe = v;        emit exeChanged(); } }
    void setArgs(const QStringList &v)   { m_args = v;            emit argsChanged(); }
    void setWorkingDir(const QString &v) { if (m_workingDir != v) { m_workingDir = v; emit workingDirChanged(); } }
    void setTransport(const QString &v)  { if (m_transport != v)  { m_transport = v;  emit transportChanged(); } }
    void setSubscribes(const QStringList &v) { m_subscribes = v;  emit subscribesChanged(); }
    void setAutoRestart(bool v)          { if (m_autoRestart != v){ m_autoRestart = v; emit autoRestartChanged(); } }
    void setRestartDelay(int v)          { if (m_restartDelay != v){ m_restartDelay = v; emit restartDelayChanged(); } }
    void setMaxRestarts(int v)           { if (m_maxRestarts != v) { m_maxRestarts = v;  emit maxRestartsChanged(); } }

    // Called from Component.onCompleted in config.qml to request auto-launch.
    Q_INVOKABLE void start() { m_autoLaunch = true; }
    bool autoLaunch() const { return m_autoLaunch; }

signals:
    void nameChanged();
    void exeChanged();
    void argsChanged();
    void workingDirChanged();
    void transportChanged();
    void subscribesChanged();
    void autoRestartChanged();
    void restartDelayChanged();
    void maxRestartsChanged();

private:
    QString m_name;
    QString m_exe;
    QStringList m_args;
    QString m_workingDir;
    QString m_transport;
    QStringList m_subscribes;
    bool m_autoLaunch  = false;
    bool m_autoRestart = false;
    int m_restartDelay = 1000;
    int m_maxRestarts = 5;
};

// ---------------------------------------------------------------------------

class TransportDef : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(QVariantMap options READ options WRITE setOptions NOTIFY optionsChanged)
public:
    explicit TransportDef(QObject *parent = nullptr) : QObject(parent) {}

    QString plugin() const { return m_plugin; }
    QVariantMap options() const { return m_options; }

    void setPlugin(const QString &v)      { if (m_plugin != v) { m_plugin = v; emit pluginChanged(); } }
    void setOptions(const QVariantMap &v) { m_options = v; emit optionsChanged(); }

    // Convenience: expose common keys as properties too
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    int port() const { return m_options.value(QStringLiteral("port"), 0).toInt(); }
    void setPort(int p) { m_options[QStringLiteral("port")] = p; emit portChanged(); emit optionsChanged(); }

signals:
    void pluginChanged();
    void optionsChanged();
    void portChanged();

private:
    QString m_plugin;
    QVariantMap m_options;
};

// ---------------------------------------------------------------------------

class GatewayDef : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(QVariantMap options READ options WRITE setOptions NOTIFY optionsChanged)
public:
    explicit GatewayDef(QObject *parent = nullptr) : QObject(parent) {}

    QString plugin() const { return m_plugin; }
    QVariantMap options() const { return m_options; }

    void setPlugin(const QString &v)      { if (m_plugin != v) { m_plugin = v; emit pluginChanged(); } }
    void setOptions(const QVariantMap &v) { m_options = v; emit optionsChanged(); }

    // Common XMPP-style properties forwarded into options map
    Q_PROPERTY(QString server READ server WRITE setServer NOTIFY serverChanged)
    Q_PROPERTY(QString user   READ user   WRITE setUser   NOTIFY userChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    QString server() const   { return m_options.value(QStringLiteral("server")).toString(); }
    QString user() const     { return m_options.value(QStringLiteral("user")).toString(); }
    QString password() const { return m_options.value(QStringLiteral("password")).toString(); }
    void setServer(const QString &v)   { m_options[QStringLiteral("server")] = v;   emit serverChanged();   emit optionsChanged(); }
    void setUser(const QString &v)     { m_options[QStringLiteral("user")] = v;     emit userChanged();     emit optionsChanged(); }
    void setPassword(const QString &v) { m_options[QStringLiteral("password")] = v; emit passwordChanged(); emit optionsChanged(); }

signals:
    void pluginChanged();
    void optionsChanged();
    void serverChanged();
    void userChanged();
    void passwordChanged();

private:
    QString m_plugin;
    QVariantMap m_options;
};

// ---------------------------------------------------------------------------

class BusConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<ProcessDef>   processes   READ processes)
    Q_PROPERTY(QQmlListProperty<TransportDef> transports  READ transports)
    Q_PROPERTY(QQmlListProperty<GatewayDef>   gateways    READ gateways)
    Q_CLASSINFO("DefaultProperty", "processes")
public:
    explicit BusConfig(QObject *parent = nullptr) : QObject(parent) {}

    QQmlListProperty<ProcessDef>   processes()  { return QQmlListProperty<ProcessDef>(this, &m_processes); }
    QQmlListProperty<TransportDef> transports() { return QQmlListProperty<TransportDef>(this, &m_transports); }
    QQmlListProperty<GatewayDef>   gateways()   { return QQmlListProperty<GatewayDef>(this, &m_gateways); }

    const QList<ProcessDef*>   &processList()   const { return m_processes; }
    const QList<TransportDef*> &transportList() const { return m_transports; }
    const QList<GatewayDef*>   &gatewayList()   const { return m_gateways; }

private:
    QList<ProcessDef*>   m_processes;
    QList<TransportDef*> m_transports;
    QList<GatewayDef*>   m_gateways;
};
