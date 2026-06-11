import ConcertoBusConfig 1.0

BusConfig {
    transports: [
        Transport { plugin: "tcp";   port: 49152 },
        Transport { plugin: "stdio" }
    ]

    // gateways: [
    //     Gateway { plugin: "xmpp"; server: "bus.example.com"; user: "bus@example.com"; password: "secret" }
    // ]

    Process {
        name: "SensorApp"
        exe: "C:/Apps/SensorApp/SensorApp.exe"
        args: ["--headless"]
        workingDir: "C:/Apps/SensorApp"
        transport: "stdio"
        subscribes: ["sensor", "control"]
        autoRestart: true
        restartDelay: 2000
        maxRestarts: 3
    }

    Process {
        name: "DisplayApp"
        exe: "C:/Apps/DisplayApp/DisplayApp.exe"
        transport: "tcp"
        subscribes: ["display"]
        autoRestart: false
    }
}
