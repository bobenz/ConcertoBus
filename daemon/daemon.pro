TEMPLATE = app
TARGET   = pm
CONFIG  += c++17
QT      += core network qml
DESTDIR  = $$OUT_PWD

INCLUDEPATH += \
    ../bus \
    ../bus/config \
    ../bus/processmanager \
    ../transports/stdio \
    ../transports/tcp

SOURCES += ../bus/main.cpp

# Legacy files (old design — BusServer/Catalog/XmppGateway — kept for reference)
OTHER_FILES += \
    ../bus/BusServer.h  ../bus/BusServer.cpp \
    ../bus/Catalog.h    ../bus/Catalog.cpp \
    ../bus/XmppGateway.h ../bus/XmppGateway.cpp

LIBS += -L$$OUT_PWD/../bus/config         -lBusConfigLib
LIBS += -L$$OUT_PWD/../bus/processmanager -lBusProcessManager
LIBS += -L$$OUT_PWD/../bus/router         -lBusRouter
LIBS += -L$$OUT_PWD/../transports/stdio   -lBusStdioTransport
LIBS += -L$$OUT_PWD/../transports/tcp     -lBusTcpTransport
LIBS += -L$$OUT_PWD/../bus/buscore        -lBusCoreLib
