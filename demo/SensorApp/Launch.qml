import ConcertoBus 1.0

LaunchSpec {
    name: "Sensor"
    transport: "stdio"
    subscribes: ["control"]
    mainQml: "App.qml"
    onCompleted: console.log("[Sensor] connected to bus and ready")
}
