TEMPLATE = lib
TARGET   = BusProcessManager
CONFIG  += staticlib c++17
QT      += core
DESTDIR  = $$OUT_PWD

INCLUDEPATH += .. ../config

HEADERS += ../ProcessManager.h
SOURCES += ../ProcessManager.cpp
