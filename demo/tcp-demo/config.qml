// Demo 2 — external TCP client drives the bus.
// pm.exe only defines Display here; it does NOT auto-launch it.
// An external client (any executable speaking the protocol) connects via TCP,
// requests the launch, and sends data.
//
// Terminal 1:  build\RelWithDebInfo\pm.exe -c demo\tcp-demo\config.qml
// Terminal 2:  build\RelWithDebInfo\client.exe demo\ControllerApp\Launch.qml
//              (or any other program implementing the bus protocol)

import ConcertoBusConfig 1.0

BusConfig {
    Process {
        id: display
        name: "Display"
        exe: "../../build/RelWithDebInfo/client.exe"
        args: ["../DisplayApp/Launch.qml"]
        workingDir: ".."
        transport: "stdio"
        subscribes: ["sensor"]
    }

    // No Component.onCompleted — Display is launched on demand by the TCP client.
}
