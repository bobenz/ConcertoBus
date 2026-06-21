import QtQml

QtObject {
    property int counter: 0

    property Timer publishTimer: Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            counter++
            busClient.publish("sensor", { value: counter, source: "Sensor" })
            console.log("[Sensor] published value:", counter)
        }
    }

    property Connections busConn: Connections {
        target: busClient
        function onMessageReceived(tag, sender, data) {
            console.log("[Sensor] received", tag, "from", sender + ":", JSON.stringify(data))
        }
    }
}
