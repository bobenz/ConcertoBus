#pragma once
#include <QObject>
#include <QStringList>

// Minimal reader for the daemon-relevant fields of an app's Launch.qml.
// pm.exe only needs name/transport/subscribes from Launch.qml — it doesn't
// use importPaths or mainQml (those are for client.exe only).
// Registered as "LaunchSpec" under "ConcertoBus 1.0" so Launch.qml files
// work identically whether loaded by the daemon or by client.exe.
class LaunchSpecReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString transport READ transport WRITE setTransport NOTIFY transportChanged)
    Q_PROPERTY(QStringList subscribes READ subscribes WRITE setSubscribes NOTIFY subscribesChanged)
    // Accept but ignore client-only fields so Load.qml parses without errors.
    Q_PROPERTY(QStringList importPaths READ importPaths WRITE setImportPaths NOTIFY importPathsChanged)
    Q_PROPERTY(QString mainQml READ mainQml WRITE setMainQml NOTIFY mainQmlChanged)
public:
    explicit LaunchSpecReader(QObject *parent = nullptr) : QObject(parent) {}

    QString     name()        const { return m_name; }
    QString     transport()   const { return m_transport; }
    QStringList subscribes()  const { return m_subscribes; }
    QStringList importPaths() const { return m_importPaths; }
    QString     mainQml()     const { return m_mainQml; }

    void setName(const QString &v)        { if (m_name != v)       { m_name = v;        emit nameChanged(); } }
    void setTransport(const QString &v)   { if (m_transport != v)  { m_transport = v;   emit transportChanged(); } }
    void setSubscribes(const QStringList &v) { m_subscribes = v;   emit subscribesChanged(); }
    void setImportPaths(const QStringList &v){ m_importPaths = v;  emit importPathsChanged(); }
    void setMainQml(const QString &v)     { if (m_mainQml != v)    { m_mainQml = v;     emit mainQmlChanged(); } }

signals:
    void nameChanged();
    void transportChanged();
    void subscribesChanged();
    void importPathsChanged();
    void mainQmlChanged();
    void completed();   // matches onCompleted: {} handler in Launch.qml (ignored by pm)

private:
    QString     m_name;
    QString     m_transport  = QStringLiteral("stdio");
    QStringList m_subscribes;
    QStringList m_importPaths;
    QString     m_mainQml    = QStringLiteral("App.qml");
};
