TEMPLATE = app
TARGET   = ConcertoBusDaemon
CONFIG  += c++17
QT      += core network qml

INCLUDEPATH += ../bus ../bus/config ../bus/processmanager ../transports/stdio

SOURCES += ../bus/main.cpp

LIBS += -L$$OUT_PWD/../bus/config           -lBusConfigLib
LIBS += -L$$OUT_PWD/../bus/processmanager   -lBusProcessManager
LIBS += -L$$OUT_PWD/../bus/router           -lBusRouter
LIBS += -L$$OUT_PWD/../transports/stdio     -lBusStdioTransport
LIBS += -L$$OUT_PWD/../bus/buscore          -lBusCoreLib
