#include <QCoreApplication>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

#include "BusClient.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    BusClient client;
    client.setName(QStringLiteral("Controller"));
    // host/port default to 127.0.0.1:49152

    int counter = 0;
    QTimer publishTimer;
    publishTimer.setInterval(1000);

    // Once Display registers on the bus, pm pushes "process_started" → launchReady.
    QObject::connect(&client, &BusClient::launchReady, [&](const QString &name) {
        qInfo() << "[Controller] process_started:" << name;
        qInfo() << "[Controller] starting to publish sensor data every second";
        publishTimer.start();
    });

    QObject::connect(&client, &BusClient::launchError, [](const QString &name, const QString &reason) {
        qWarning() << "[Controller] launch failed for" << name << ":" << reason;
    });

    QObject::connect(&publishTimer, &QTimer::timeout, [&]() {
        ++counter;
        client.publish(QStringLiteral("sensor"),
                       QJsonObject{{QStringLiteral("value"),  counter},
                                   {QStringLiteral("source"), QStringLiteral("Controller")}});
        qInfo() << "[Controller] published sensor value:" << counter;
    });

    QObject::connect(&client, &BusClient::connectedChanged, [&]() {
        if (client.isConnected()) {
            qInfo() << "[Controller] connected to bus — sending launch Display";
            client.launch(QStringLiteral("Display"));
        }
    });

    QObject::connect(&client, &BusClient::errorOccurred, [](const QString &err) {
        qWarning() << "[Controller] error:" << err;
    });

    client.connectToBus();
    return app.exec();
}
