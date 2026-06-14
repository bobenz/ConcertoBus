// Demo 1 -- stdio auto-launch + TCP listener.
// pm.exe auto-launches SensorApp and DisplayApp via stdio pipes.
// Any external client (e.g. Qt5ClientApp) can connect via TCP on port 49152.
//
// Run from the project root:
//   build\RelWithDebInfo\pm.exe -c demo\config.qml

import QtQml 2.0
import ConcertoBusConfig 1.0

BusConfig {
    transports: [
        Transport { plugin: "tcp"; port: 49152 }
    ]

    App { id: sns; path: "SensorApp" }
    App { id: dsp; path: "DisplayApp" }

    Component.onCompleted: {
        sns.start()
        dsp.start()
    }
}
