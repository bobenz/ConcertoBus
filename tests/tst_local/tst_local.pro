TEMPLATE = app
TARGET   = tst_local
CONFIG  += testcase c++17
QT      += core network testlib

INCLUDEPATH += ../../bus ../../client

SOURCES += \
    ../tst_local.cpp \
    ../../bus/BusServer.cpp \
    ../../bus/Router.cpp \
    ../../bus/Catalog.cpp

HEADERS += \
    ../../bus/BusServer.h \
    ../../bus/Router.h \
    ../../bus/Catalog.h

LIBS += -L$$OUT_PWD/../../client -lConcertoBusClient
