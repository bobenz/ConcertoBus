#pragma once
#include <QByteArray>
#include <QFile>
#include <QJsonObject>
#include <QObject>
#include <QtQml/qqml.h>

class QThread;

// Client for processes launched by the daemon via StdioTransport.
// Reads from stdin and writes to stdout using the same newline-delimited JSON
// protocol as the TCP-based BusClient.  Intended to be used via the
// ConcertoBus QML plugin inside launched child processes.
class StdioBusClient : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool registered READ isRegistered NOTIFY registeredChanged)

public:
    explicit StdioBusClient(QObject *parent = nullptr);
    ~StdioBusClient() override;

    QString name() const { return m_name; }
    void setName(const QString &name);

    bool isRegistered() const { return m_registered; }

    Q_INVOKABLE void connectToBus();    // starts stdin reader, sends register
    Q_INVOKABLE void subscribe(const QString &tag);
    Q_INVOKABLE void unsubscribe(const QString &tag);
    Q_INVOKABLE void publish(const QString &to, const QJsonObject &data);

signals:
    void nameChanged();
    void registeredChanged();
    void messageReceived(const QString &from, const QString &sender,
                         const QJsonObject &data);
    void errorOccurred(const QString &error);

private slots:
    void processLine(const QByteArray &line);

private:
    void sendJson(const QJsonObject &obj);
    void startReading();

    QString    m_name;
    bool       m_registered = false;
    QFile     *m_stdout = nullptr;
    QThread   *m_thread = nullptr;
};
