TEMPLATE = app
TARGET   = client
CONFIG  += c++17
QT      += core qml
DESTDIR  = $$OUT_PWD

INCLUDEPATH += ..

SOURCES += main.cpp

LIBS += -L$$OUT_PWD/../ -lConcertoBusClient
