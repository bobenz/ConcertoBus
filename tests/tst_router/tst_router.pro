TEMPLATE = app
TARGET   = tst_router
CONFIG  += testcase c++17
QT      += core testlib

MOC_DIR     = $$OUT_PWD
INCLUDEPATH += $$OUT_PWD ../../bus

SOURCES += ../tst_router.cpp

LIBS += -L$$OUT_PWD/../../bus/router -lBusRouter
