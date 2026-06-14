import QtQml

QtObject {
    property Connections busConn: Connections {
        target: busClient
        function onMessageReceived(tag, sender, data) {
            console.log("[Display] sensor reading from", sender + ":", JSON.stringify(data))
        }
        function onConnectedChanged() {
            if (busClient.connected)
                console.log("[Display] registered on bus as:", busClient.name)
        }
    }
}
