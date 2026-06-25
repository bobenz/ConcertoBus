// XMPP Demo — Runner B (machine B)
// Hosts DisplayApp locally. Federates with Runner A over XMPP so that
// Sensor messages published on Runner A are received here.
//
// Run (from project root):
//   build\xmpp\RelWithDebInfo\pm.exe -c demo\xmpp-demo\runner-b\config.qml

import QtQml 2.0
import ConcertoBusConfig 1.0

BusConfig {
    transports: [
        Transport { plugin: "tcp"; port: 49152 }
    ]

    gateways: [
        Gateway {
            plugin:   "xmpp"
            user:     "client2@xmpp.credics"
            password: "test2"
            peers:    ["client1@xmpp.credics"]
        }
    ]

    App { id: dsp; path: "../DisplayApp" }

    Component.onCompleted: dsp.start()
}
