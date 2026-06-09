#include "BusServer.h"
#include "Catalog.h"
#ifdef CONCERTO_BUS_XMPP
#include "XmppGateway.h"
#endif

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
#ifdef CONCERTO_BUS_XMPP
    parser.addOption({{"j","jid"}, "XMPP JID for federation (e.g. bus@example.com)", "jid"});
    parser.addOption({{"P","xmpp-password"}, "XMPP password", "password"});
    parser.addOption({{"H","xmpp-host"}, "XMPP server host (optional)", "host"});
    parser.addOption({{"X","xmpp-port"}, "XMPP server port (default 5222)", "xmpp-port", "5222"});
#endif
    parser.process(app);

    const quint16 port = static_cast<quint16>(parser.value("port").toUInt());
    const QString catalogPath = parser.value("catalog");

    Catalog catalog;
    if (!catalogPath.isEmpty())
        catalog.load(catalogPath);

    BusServer server;
    server.setCatalog(&catalog);

#ifdef CONCERTO_BUS_XMPP
    const QString jid = parser.value("jid");
    if (!jid.isEmpty()) {
        auto *gw = new XmppGateway(server.router(), &app);
        server.setXmppGateway(gw);
        const QString xmppPass = parser.value("xmpp-password");
        const QString xmppHost = parser.value("xmpp-host");
        const int xmppPort = parser.value("xmpp-port").toInt();
        gw->connectToServer(jid, xmppPass, xmppHost, xmppPort);
    }
#endif

    if (!server.listen(port))
        return 1;

    return app.exec();
}
