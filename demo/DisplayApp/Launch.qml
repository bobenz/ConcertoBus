import ConcertoBus 1.0

LaunchSpec {
    name: "Display"
    transport: "stdio"
    subscribes: ["sensor"]
    mainQml: "App.qml"
    onCompleted: console.log("[Display] connected to bus, listening for 'sensor'")
}
