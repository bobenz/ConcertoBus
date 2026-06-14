// Demo 1 — stdio only.
// pm.exe launches both Sensor and Display automatically via Component.onCompleted.
//
// Run from the project root:
//   build\RelWithDebInfo\pm.exe -c demo\config.qml

import ConcertoBusConfig 1.0

BusConfig {
    Process {
        id: sensor
        name: "Sensor"
        exe: "../build/RelWithDebInfo/client.exe"
        args: ["SensorApp/Launch.qml"]
        workingDir: "."
        transport: "stdio"
        subscribes: ["control"]
    }

    Process {
        id: display
        name: "Display"
        exe: "../build/RelWithDebInfo/client.exe"
        args: ["DisplayApp/Launch.qml"]
        workingDir: "."
        transport: "stdio"
        subscribes: ["sensor"]
    }

    Component.onCompleted: {
        sensor.start()
        display.start()
    }
}
