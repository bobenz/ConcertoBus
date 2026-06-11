TEMPLATE = app
TARGET   = ConcertoBusDaemon
CONFIG  += c++17
QT      += core network qml

INCLUDEPATH += ../bus ../bus/config

HEADERS += \
    ../bus/IBusTransport.h \
    ../bus/IBusGateway.h \
    ../bus/BusServer.h \
    ../bus/Router.h \
    ../bus/Catalog.h

SOURCES += \
    ../bus/main.cpp \
    ../bus/BusServer.cpp \
    ../bus/Router.cpp \
    ../bus/Catalog.cpp

LIBS += -L$$OUT_PWD/../bus/config -lBusConfigLib
LIBS += -L$$OUT_PWD/../bus/processmanager -lBusProcessManager
