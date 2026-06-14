// Demo 2 — external TCP client drives the bus.
// pm.exe registers Display but does not auto-launch it.
// An external client connects via TCP, requests launch, and sends data.
//
// Terminal 1:  build\RelWithDebInfo\pm.exe -c demo\tcp-demo\config.qml
// Terminal 2:  build\RelWithDebInfo\client.exe demo\ControllerApp\Launch.qml

import ConcertoBusConfig 1.0

BusConfig {
    App {
        id: dsp
        path: "../DisplayApp"
    }

    // No Component.onCompleted — Display is launched on demand by the TCP client.
}
