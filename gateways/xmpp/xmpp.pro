TEMPLATE = lib
TARGET   = XmppGateway
CONFIG  += plugin c++17
QT      += core network

QXMPP_DIR = D:/Qt/qxmpp-installed
INCLUDEPATH += ../../bus $$QXMPP_DIR/include
LIBS        += -L$$QXMPP_DIR/lib -lQXmppQt6

DESTDIR  = $$OUT_PWD/../../gateways

HEADERS += ../../bus/IBusGateway.h XmppGateway.h
SOURCES += ../../bus/IBusGateway.cpp XmppGateway.cpp

OTHER_FILES += XmppGateway.json
