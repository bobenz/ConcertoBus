// XMPP Demo — Runner A (machine A)
// Hosts SensorApp locally. Federates with Runner B over XMPP so that
// DisplayApp on Runner B receives Sensor's published messages.
//
// Run (from project root):
//   build\RelWithDebInfo\pm.exe -c demo\xmpp-demo\runner-a\config.qml

import QtQml 2.0
import ConcertoBusConfig 1.0

BusConfig {
    transports: [
        Transport { plugin: "tcp"; port: 49152 }
    ]

    gateways: [
        Gateway {
            plugin:   "xmpp"
            user:     "client1@xmpp.credics"
            password: "test1"
            peers:    ["client2@xmpp.credics"]
        }
    ]

    App { id: sns; path: "../SensorApp" }

    Component.onCompleted: sns.start()
}
