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
    daemon \
    tests/tst_config \
    tests/tst_processmanager \
    tests/tst_router \
    tests/tst_stdio \
    tests/tst_buscore \
    tests/stdio_echo_helper

# Uncomment when implemented:
# SUBDIRS += transports/websocket
# SUBDIRS += gateways/xmpp
