// Demo 1 — stdio only.
// pm.exe reads Launch.qml from each app directory and auto-launches both.
//
// Run from the project root:
//   build\RelWithDebInfo\pm.exe -c demo\config.qml

import QtQml
import ConcertoBusConfig 1.0

BusConfig {
    App {
        id: sns
        path: "SensorApp"
    }
    App {
        id: dsp
        path: "DisplayApp"
    }

    Component.onCompleted: {
        sns.start()
        dsp.start()
    }
}
