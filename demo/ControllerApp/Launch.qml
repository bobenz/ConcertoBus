import ConcertoBus 1.0

LaunchSpec {
    name: "Controller"
    transport: "tcp"
    subscribes: []
    mainQml: "App.qml"
    onCompleted: console.log("[Controller] connected to bus via TCP")
}
