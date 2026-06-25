// Demo: QML injection
//
// SensorApp  — publishes a counter every second (stdio)
// ShellApp   — opens a window; click "Load Sensor Panel" to inject the widget
// SensorPanel — injected into ShellApp on demand; displays the live counter
//
// Run from the project root:
//   build\RelWithDebInfo\pm.exe -c demo\config.qml

import QtQml 2.0
import ConcertoBusConfig 1.0

BusConfig {
    App { id: sns;   path: "SensorApp" }
    App { id: shell; path: "ShellApp" }
    App { id: panel; path: "SensorPanel" }   // launched on demand from ShellApp button

    Component.onCompleted: {
        sns.start()
        shell.start()
    }
}
