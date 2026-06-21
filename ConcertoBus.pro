TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS  = \
    bus/config \
    bus/processmanager \
    bus/router \
    transports/stdio \
    transports/tcp \
    bus/buscore \
    client \
    client/host \
    client/qmlplugin \
    daemon \
    tests/tst_config \
    tests/tst_processmanager \
    tests/tst_router \
    tests/stdio_echo_helper \
    tests/tst_stdio \
    tests/tst_buscore \
    tests/tst_qt5client \
    tests/tst_local \
    demo/Qt5ClientApp

# Uncomment when implemented:
# SUBDIRS += transports/websocket
# SUBDIRS += gateways/xmpp
