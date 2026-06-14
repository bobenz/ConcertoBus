TEMPLATE = lib
TARGET   = ConcertoBusClient
CONFIG  += staticlib c++17
QT      += core network qml
DESTDIR  = $$OUT_PWD

INCLUDEPATH += .

HEADERS += \
    AbstractBusClient.h \
    BusClient.h \
    LaunchSpec.h \
    StdioBusClient.h

SOURCES += \
    AbstractBusClient.cpp \
    BusClient.cpp \
    LaunchSpec.cpp \
    StdioBusClient.cpp
