TEMPLATE = lib
TARGET   = BusConfigLib
CONFIG  += staticlib c++17
QT      += core qml
DESTDIR  = $$OUT_PWD

INCLUDEPATH += . ..

HEADERS += \
    BusConfigTypes.h \
    BusConfigLoader.h

SOURCES += \
    BusConfigLoader.cpp
