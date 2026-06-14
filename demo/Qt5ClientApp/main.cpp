// Qt5-compatible ConcertoBus client demo.
// Connects via TCP, subscribes to "sensor", prints every message it receives.
//
// Run against demo 1 or demo 2:
//   build\RelWithDebInfo\pm.exe -c demo\config.qml
//   build\RelWithDebInfo\Qt5ClientApp.exe

#include "RawBusClient.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    RawBusClient client("Qt5Listener");

    QObject::connect(&client, &RawBusClient::connected, [&]() {
        fprintf(stderr, "[Qt5Client] registered on bus as: %s\n", qPrintable(client.name()));
        fflush(stderr);
        client.subscribe("sensor");
    });

    QObject::connect(&client, &RawBusClient::messageReceived,
        [](const QString &tag, const QString &sender, const QJsonObject &data) {
            fprintf(stderr, "[Qt5Client] %s from %s: %s\n",
                    qPrintable(tag), qPrintable(sender),
                    QJsonDocument(data).toJson(QJsonDocument::Compact).constData());
            fflush(stderr);
        });

    QObject::connect(&client, &RawBusClient::errorOccurred, [](const QString &e) {
        fprintf(stderr, "[Qt5Client] error: %s\n", qPrintable(e));
        fflush(stderr);
    });

    QObject::connect(&client, &RawBusClient::disconnected, &app, &QCoreApplication::quit);

    client.connectToBus();
    return app.exec();
}
