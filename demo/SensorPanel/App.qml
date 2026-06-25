import QtQuick 2.15

// Injected into ShellApp. Subscribes to "sensor" using ShellApp's busClient
// and displays the live counter value.
Rectangle {
    width: parent ? parent.width : 400
    height: 72
    radius: 6
    color: "#1e1e2e"
    border.color: "#89b4fa"
    border.width: 1

    property int value: 0

    Component.onCompleted: busClient.subscribe("sensor")

    Connections {
        target: busClient
        function onMessageReceived(tag, sender, data) {
            if (tag === "sensor" && data.value !== undefined)
                value = data.value
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: 20

        Text {
            text: "Sensor"
            color: "#89b4fa"
            font.pixelSize: 18
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: value
            color: "#cdd6f4"
            font.pixelSize: 36
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
