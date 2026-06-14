TEMPLATE = app
TARGET   = tst_buscore
CONFIG  += testcase c++17
QT      += core network testlib
DESTDIR  = $$OUT_PWD/..
MOC_DIR  = $$OUT_PWD
INCLUDEPATH += $$OUT_PWD ../../bus ../../transports/stdio

SOURCES += ../tst_buscore.cpp

LIBS += -L$$OUT_PWD/../../bus/buscore       -lBusCoreLib
LIBS += -L$$OUT_PWD/../../bus/processmanager -lBusProcessManager
LIBS += -L$$OUT_PWD/../../bus/config        -lBusConfigLib
LIBS += -L$$OUT_PWD/../../bus/router        -lBusRouter
LIBS += -L$$OUT_PWD/../../transports/stdio  -lBusStdioTransport

