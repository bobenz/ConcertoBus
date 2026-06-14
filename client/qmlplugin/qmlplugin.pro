TEMPLATE = lib
TARGET   = concertobusplugin
CONFIG  += plugin c++17
QT      += core network qml quick
DESTDIR  = $$OUT_PWD/../../imports/ConcertoBus

QML_IMPORT_NAME          = ConcertoBus
QML_IMPORT_MAJOR_VERSION = 1

INCLUDEPATH += ..

HEADERS += \
    ../AbstractBusClient.h \
    ../BusClient.h \
    ../LaunchSpec.h \
    ../StdioBusClient.h

SOURCES += \
    ../AbstractBusClient.cpp \
    ../BusClient.cpp \
    ../LaunchSpec.cpp \
    ../StdioBusClient.cpp \
    ../ConcertoBusPlugin.cpp
