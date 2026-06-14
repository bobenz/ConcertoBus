TEMPLATE = app
TARGET   = tst_config
CONFIG  += testcase c++17
QT      += core qml testlib

MOC_DIR     = $$OUT_PWD
INCLUDEPATH += $$OUT_PWD ../../bus ../../bus/config

SOURCES += ../tst_config.cpp

LIBS += -L$$OUT_PWD/../../bus/config -lBusConfigLib
