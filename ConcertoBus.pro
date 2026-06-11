TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS  = \
    bus/config \
    bus/processmanager \
    client \
    daemon \
    tests/tst_config \
    tests/tst_processmanager \
    tests/tst_local

# Optional plugins (uncomment when implemented)
# SUBDIRS += transports/tcp
# SUBDIRS += transports/websocket
# SUBDIRS += gateways/xmpp
