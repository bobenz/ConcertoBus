TEMPLATE = lib
TARGET   = TcpTransport
CONFIG  += plugin c++17
QT      += core network
DESTDIR  = $$OUT_PWD/../../plugins

INCLUDEPATH += ../../bus

HEADERS += ../../bus/IBusTransport.h TcpTransport.h
SOURCES += ../../bus/IBusTransport.cpp TcpTransport.cpp

OTHER_FILES += TcpTransport.json
