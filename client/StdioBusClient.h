#pragma once
#include "AbstractBusClient.h"
#include <QFile>
#include <QtQml/qqml.h>

class QThread;

class StdioBusClient : public AbstractBusClient
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit StdioBusClient(QObject *parent = nullptr);
    ~StdioBusClient() override;

    // AbstractBusClient
    QString name() const override { return m_name; }
    void setName(const QString &name) override;
    bool isConnected() const override { return m_registered; }

    Q_INVOKABLE void connectToBus() override;
    Q_INVOKABLE void subscribe(const QString &tag) override;
    Q_INVOKABLE void unsubscribe(const QString &tag) override;
    Q_INVOKABLE void publish(const QString &to, const QJsonObject &data) override;
    Q_INVOKABLE void launch(const QString &name) override;
    Q_INVOKABLE void injectQml(const QString &target, const QString &name,
                               const QString &url = {}, const QString &source = {}) override;

private slots:
    void processLine(const QByteArray &line);

private:
    void sendJson(const QJsonObject &obj);
    void startReading();

    QString  m_name;
    bool     m_registered = false;
    QFile   *m_stdout = nullptr;
    QThread *m_thread = nullptr;
};
