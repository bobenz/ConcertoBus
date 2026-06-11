TEMPLATE = lib
TARGET   = BusStdioTransport
CONFIG  += staticlib c++17
QT      += core

INCLUDEPATH += ../../bus

HEADERS += StdioTransport.h
SOURCES += StdioTransport.cpp ../../bus/IBusTransport.cpp
