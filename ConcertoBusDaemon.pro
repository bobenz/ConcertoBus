TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS = \
    bus/config \
    bus/processmanager \
    bus/router \
    transports/stdio \
    transports/tcp \
    gateways/xmpp \
    bus/buscore \
    daemon \
    tests/stdio_echo_helper \
    tests/tst_config \
    tests/tst_processmanager \
    tests/tst_router \
    tests/tst_stdio \
    tests/tst_buscore \
    tests/tst_qt5client
