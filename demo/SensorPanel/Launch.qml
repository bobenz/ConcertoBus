import ConcertoBus 1.0

// Injects SensorPanel into an already-running ShellApp.
// The daemon launches this via stdio; the process sends the inject and exits.
// Trigger with:  busClient.launch("SensorPanel")
LaunchSpec {
    name: "SensorPanel"
    transport: "stdio"
    attachTo: "ShellApp"
    mainQml: "App.qml"
}
