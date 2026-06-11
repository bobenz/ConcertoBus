TEMPLATE = app
TARGET   = tst_router
CONFIG  += testcase c++17
QT      += core testlib

INCLUDEPATH += ../../bus

SOURCES += ../tst_router.cpp

LIBS += -L$$OUT_PWD/../../bus/router -lBusRouter
