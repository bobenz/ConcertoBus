TEMPLATE = app
TARGET   = stdio_echo_helper
CONFIG  += c++17
QT      += core network qml
DESTDIR  = $$OUT_PWD/..

INCLUDEPATH += ../../client

SOURCES += main.cpp

LIBS += -L$$OUT_PWD/../../client -lConcertoBusClient
