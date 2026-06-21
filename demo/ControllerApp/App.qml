import QtQml

QtObject {
    property int counter: 0

    property Timer publishTimer: Timer {
        interval: 1000
        running: false
        repeat: true
        onTriggered: {
            counter++
            busClient.publish("sensor", { value: counter, source: busClient.name })
            console.log("[Controller] published sensor value:", counter)
        }
    }

    property Connections busConn: Connections {
        target: busClient

        function onConnectedChanged() {
            if (busClient.connected) {
                console.log("[Controller] connected — sending launch Display")
                busClient.launch("Display")
            }
        }

        function onLaunchReady(name) {
            console.log("[Controller] process_started:", name, "— starting to publish")
            publishTimer.running = true
        }

        function onLaunchError(name, reason) {
            console.log("[Controller] launch failed for", name + ":", reason)
        }
    }
}
