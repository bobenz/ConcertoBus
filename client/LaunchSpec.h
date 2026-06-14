#pragma once
#include <QObject>
#include <QStringList>
#include <QtQml/qqml.h>

// Self-describing application descriptor, loaded from Launch.qml.
// The daemon reads it to discover the process; ConcertoBusHost reads it
// to configure the bus client and load the main QML component.
class LaunchSpec : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString transport READ transport WRITE setTransport NOTIFY transportChanged)
    Q_PROPERTY(QStringList subscribes READ subscribes WRITE setSubscribes NOTIFY subscribesChanged)
    Q_PROPERTY(QStringList importPaths READ importPaths WRITE setImportPaths NOTIFY importPathsChanged)
    Q_PROPERTY(QString mainQml READ mainQml WRITE setMainQml NOTIFY mainQmlChanged)

public:
    explicit LaunchSpec(QObject *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &v);

    QString transport() const { return m_transport; }
    void setTransport(const QString &v);

    QStringList subscribes() const { return m_subscribes; }
    void setSubscribes(const QStringList &v);

    QStringList importPaths() const { return m_importPaths; }
    void setImportPaths(const QStringList &v);

    QString mainQml() const { return m_mainQml; }
    void setMainQml(const QString &v);

    // Called by ConcertoBusHost after the main component is loaded.
    void emitCompleted() { emit completed(); }

signals:
    void nameChanged();
    void transportChanged();
    void subscribesChanged();
    void importPathsChanged();
    void mainQmlChanged();
    void completed();   // maps to onCompleted: {} in QML

private:
    QString     m_name;
    QString     m_transport  = QStringLiteral("stdio");
    QStringList m_subscribes;
    QStringList m_importPaths;
    QString     m_mainQml    = QStringLiteral("App.qml");
};
