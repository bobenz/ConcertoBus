TEMPLATE = lib
TARGET   = BusTcpTransport
CONFIG  += staticlib c++17
QT      += core network
DESTDIR  = $$OUT_PWD

INCLUDEPATH += ../../bus

HEADERS += TcpTransport.h
SOURCES += TcpTransport.cpp
