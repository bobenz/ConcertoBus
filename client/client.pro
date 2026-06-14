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

# Legacy QML wrappers (superseded by QML_ELEMENT in the classes above)
OTHER_FILES += \
    BusClientQml.h \
    BusClientQml.cpp \
    ConcertoBusPlugin.cpp
