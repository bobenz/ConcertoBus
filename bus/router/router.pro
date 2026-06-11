TEMPLATE = lib
TARGET   = BusRouter
CONFIG  += staticlib c++17
QT      += core
DESTDIR  = $$OUT_PWD

INCLUDEPATH += .. ..

HEADERS += ../Router.h ../IBusTransport.h
SOURCES += ../Router.cpp
