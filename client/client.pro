TEMPLATE = lib
TARGET   = ConcertoBusClient
CONFIG  += staticlib c++17
QT      += core network
DESTDIR  = $$OUT_PWD

INCLUDEPATH += .

HEADERS += BusClient.h
SOURCES += BusClient.cpp
