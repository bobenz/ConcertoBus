TEMPLATE = app
TARGET   = tst_processmanager
CONFIG  += testcase c++17
QT      += core qml testlib

INCLUDEPATH += ../../bus ../../bus/config

SOURCES += ../tst_processmanager.cpp

LIBS += -L$$OUT_PWD/../../bus/processmanager -lBusProcessManager
LIBS += -L$$OUT_PWD/../../bus/config -lBusConfigLib
