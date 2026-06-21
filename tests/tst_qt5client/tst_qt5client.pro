TEMPLATE = app
TARGET   = tst_qt5client
CONFIG  += testcase c++17
QT      += core network testlib
DESTDIR  = $$OUT_PWD/..

MOC_DIR     = $$OUT_PWD
INCLUDEPATH += $$OUT_PWD ../../bus ../../transports/stdio \
               ../../demo/Qt5ClientApp

# RawBusClient is a header-only QObject — must be listed in HEADERS so qmake
# generates its moc file.
HEADERS += ../../demo/Qt5ClientApp/RawBusClient.h
SOURCES += ../tst_qt5client.cpp

LIBS += -L$$OUT_PWD/../../bus/buscore        -lBusCoreLib
LIBS += -L$$OUT_PWD/../../bus/processmanager -lBusProcessManager
LIBS += -L$$OUT_PWD/../../bus/config         -lBusConfigLib
LIBS += -L$$OUT_PWD/../../bus/router         -lBusRouter
LIBS += -L$$OUT_PWD/../../transports/stdio   -lBusStdioTransport
