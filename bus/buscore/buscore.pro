TEMPLATE = lib
TARGET   = BusCoreLib
CONFIG  += staticlib c++17
QT      += core

INCLUDEPATH += .. ../config ../processmanager ../../transports/stdio

HEADERS += ../BusCore.h
SOURCES += ../BusCore.cpp
