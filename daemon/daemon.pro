TEMPLATE = app
TARGET   = ConcertoBusDaemon
CONFIG  += c++17
QT      += core network qml

INCLUDEPATH += ../bus ../bus/config

HEADERS += \
    ../bus/IBusTransport.h \
    ../bus/IBusGateway.h \
    ../bus/Router.h

SOURCES += \
    ../bus/main.cpp \
    ../bus/Router.cpp

LIBS += -L$$OUT_PWD/../bus/config       -lBusConfigLib
LIBS += -L$$OUT_PWD/../bus/processmanager -lBusProcessManager
LIBS += -L$$OUT_PWD/../bus/router       -lBusRouter
