TEMPLATE = lib
TARGET   = BusCoreLib
CONFIG  += staticlib c++17
QT      += core
DESTDIR  = $$OUT_PWD

INCLUDEPATH += .. ../config ../processmanager ../../transports/stdio

HEADERS += ../BusCore.h
SOURCES += ../BusCore.cpp
