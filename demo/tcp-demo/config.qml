// Demo 2 -- TCP on-demand launch.
// pm.exe knows about DisplayApp but does not start it automatically.
// An external TCP client (Controller) connects, requests launch, and publishes data.
// Qt5ClientApp can also connect and receive "sensor" messages.
//
// Terminal 1:  build\RelWithDebInfo\pm.exe -c demo\tcp-demo\config.qml
// Terminal 2:  build\RelWithDebInfo\client.exe demo\ControllerApp\Launch.qml

import QtQml 2.0
import ConcertoBusConfig 1.0

BusConfig {
    transports: [
        Transport { plugin: "tcp"; port: 49152 }
    ]

    App { id: dsp; path: "../DisplayApp" }

    // No Component.onCompleted -- Display is launched on demand by the TCP client.
}
