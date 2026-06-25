import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

ApplicationWindow {
    visible: true
    width: 500
    height: 300
    title: "Shell"

    // Route inject pushes from the bus into live QML objects.
    Connections {
        target: busClient
        function onInjectionRequested(name, url, source) {
            var comp = url ? Qt.createComponent(url)
                           : Qt.createQmlObject(source, panels, name)
            if (!comp || !comp.status) return   // createQmlObject path: done
            if (comp.status === Component.Ready) {
                comp.createObject(panels)
            } else if (comp.status === Component.Loading) {
                comp.statusChanged.connect(function() {
                    if (comp.status === Component.Ready)
                        comp.createObject(panels)
                    else if (comp.status === Component.Error)
                        console.error("inject error:", comp.errorString())
                })
            } else {
                console.error("inject error:", comp.errorString())
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Button {
            text: "Load Sensor Panel"
            Layout.alignment: Qt.AlignHCenter
            onClicked: busClient.launch("SensorPanel")
        }

        // Injected panels land here
        Column {
            id: panels
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
        }
    }
}
