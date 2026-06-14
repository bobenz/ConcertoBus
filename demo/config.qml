// Demo configuration for ConcertoBusDaemon (pm.exe).
// All paths are relative to the directory of this file.
// client.exe is expected one level up in build/RelWithDebInfo/ or build/.
//
// Run from the project root:
//   build\RelWithDebInfo\pm.exe -c demo\config.qml --launch Sensor --launch Display

import ConcertoBusConfig 1.0

BusConfig {
    processes: [
        ProcessDef {
            name: "Sensor"
            exe: "../build/RelWithDebInfo/client.exe"
            args: ["SensorApp/Launch.qml"]
            workingDir: "."
            transport: "stdio"
            subscribes: ["control"]
        },
        ProcessDef {
            name: "Display"
            exe: "../build/RelWithDebInfo/client.exe"
            args: ["DisplayApp/Launch.qml"]
            workingDir: "."
            transport: "stdio"
            subscribes: ["sensor"]
        }
    ]
}
