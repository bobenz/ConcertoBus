#include "BusServer.h"
#include "Catalog.h"

#include <QCoreApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("ConcertoBusDaemon");
    app.setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"p","port"}, "TCP port to listen on (default 49152)", "port", "49152"});
    parser.addOption({{"c","catalog"}, "Path to launcher_catalog.json", "catalog"});
    parser.process(app);

    const quint16 port = static_cast<quint16>(parser.value("port").toUInt());
    const QString catalogPath = parser.value("catalog");

    Catalog catalog;
    if (!catalogPath.isEmpty())
        catalog.load(catalogPath);

    BusServer server;
    server.setCatalog(&catalog);

    if (!server.listen(port))
        return 1;

    return app.exec();
}
