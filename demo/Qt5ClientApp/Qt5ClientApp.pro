TEMPLATE = app
TARGET   = Qt5ClientApp
# c++11 minimum for Qt 5; Qt 6 will use its own standard via the kit
CONFIG  += c++11
QT      += core network
DESTDIR  = $$OUT_PWD

HEADERS += RawBusClient.h
SOURCES += main.cpp
