# ConcertoBus Developer Guide

## Architecture in one paragraph

The daemon (`pm.exe`) is a message broker and process manager. It speaks a
newline-delimited JSON protocol over TCP (port 49152 by default) or over the
stdin/stdout pipes of child processes it launches. Every participant registers
a unique name, subscribes to tags, and publishes messages. The daemon routes
each published message to every subscriber of that tag. Nothing else — no
schema, no types, no RPC framework.

```
  SensorApp ──stdio──► pm.exe ──stdio──► ShellApp
                │                   ▲
                │   TCP             │
                └───────► Qt5Client─┘
```

---

## Wire protocol

All messages are UTF-8 JSON objects terminated by `\n`. No framing, no length
prefix, no binary.

### Client → Daemon

| Field | Required | Description |
|-------|----------|-------------|
| `cmd` | yes | One of `register` `subscribe` `unsubscribe` `publish` `launch` `kill` `restart` `inject` |

#### `register` — must be the very first message

```json
{"cmd":"register","name":"SensorApp"}
```

Reply: `{"ok":true}` or `{"error":"name_taken"}`

#### `subscribe`

```json
{"cmd":"subscribe","tag":"sensor"}
```

Reply: `{"ok":true}`

Tag format: plain string, e.g. `"sensor"`, `"control"`, `"ui.button"`.

#### `unsubscribe`

```json
{"cmd":"unsubscribe","tag":"sensor"}
```

#### `publish`

```json
{"cmd":"publish","to":"sensor","data":{"value":42}}
```

`"to"` is either:
- `"tagname"` — broadcast to all subscribers of that tag
- `"tagname:TargetName"` — unicast to a specific registered name that also subscribes to the tag

If no subscriber is online, the message is queued and delivered when one subscribes.

#### `launch` / `kill` / `restart`

```json
{"cmd":"launch","name":"DisplayApp"}
{"cmd":"kill","name":"DisplayApp"}
{"cmd":"restart","name":"DisplayApp"}
```

The sender becomes a **watcher** for that process. Async replies are pushed back:

```json
{"event":"launching","name":"DisplayApp"}
{"event":"process_started","name":"DisplayApp"}
{"event":"process_stopped","name":"DisplayApp"}
{"event":"process_crashed","name":"DisplayApp"}
{"event":"injected","name":"SensorPanel"}
```

`launch` returns `{"event":"process_started"}` immediately if the process is already running.

#### `inject` — push QML into a running client engine

```json
{"cmd":"inject","target":"ShellApp","name":"MyPanel","url":"file:///D:/apps/Panel.qml"}
{"cmd":"inject","target":"ShellApp","name":"Alert","source":"import QtQuick 2.0; Text { text:'hello' }"}
```

Exactly one of `url` or `source` must be non-empty. The daemon forwards the
message directly to the target client's socket.

---

### Daemon → Client (push messages)

#### Normal message delivery

```json
{"push":true,"from":"sensor","sender":"SensorApp","data":{"value":42}}
```

| Field | Description |
|-------|-------------|
| `from` | the tag that was subscribed |
| `sender` | registered name of the publisher |
| `data` | the payload object from the `publish` command |

#### Inject delivery

```json
{"push":true,"from":"__inject__","sender":"Controller","data":{"inject":true,"name":"MyPanel","url":"..."}}
```

Received as `onInjectionRequested(name, url, source)` on `busClient`.

#### Error replies

```json
{"error":"not_registered"}
{"error":"name_taken"}
{"error":"unknown_process","name":"Foo"}
{"error":"target_not_found","name":"ShellApp"}
{"error":"missing_target_or_name"}
```

---

## QML application anatomy

Every app consists of two files living in the same directory:

```
MyApp/
  Launch.qml    ← descriptor read by pm.exe and by client.exe
  App.qml       ← the actual UI / logic
```

### `Launch.qml`

```qml
import ConcertoBus 1.0

LaunchSpec {
    name:        "MyApp"        // unique bus registration name
    transport:   "stdio"        // "stdio" (default) or "tcp"
    subscribes:  ["sensor"]     // tags to subscribe on startup
    importPaths: []             // extra QML import search paths (relative to this dir)
    mainQml:     "App.qml"      // which file to load as the root component

    // Optional: inject into an existing host rather than spawning a new window.
    // attachTo:  "ShellApp"

    onCompleted: {
        // Called after App.qml is loaded and the bus client is connected.
    }
}
```

When `attachTo` is set, `pm.exe` does **not** spawn a new process when
`launch("MyApp")` is called. Instead it sends an `inject` push directly to
the named running client.

### `App.qml`

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15

// busClient is injected into QML context by client.exe
// It is an AbstractBusClient instance (StdioBusClient or BusClient).

Item {
    // Subscribe at runtime (supplements subscribes[] in Launch.qml)
    Component.onCompleted: busClient.subscribe("control")

    // Receive messages
    Connections {
        target: busClient
        function onMessageReceived(tag, sender, data) {
            if (tag === "sensor")
                console.log("value:", data.value, "from:", sender)
        }
    }

    // Publish
    Button {
        text: "Send"
        onClicked: busClient.publish("control", { action: "stop" })
    }
}
```

---

## `busClient` API reference

`busClient` is set as a QML context property by `client.exe`. Its type is
`AbstractBusClient` — the same interface whether transport is stdio or TCP.

```qml
// Properties
busClient.name        // string — registered name (read/write)
busClient.connected   // bool — true after {"ok":true} from daemon

// Methods (all Q_INVOKABLE, callable from QML and C++)
busClient.connectToBus()
busClient.subscribe(tag)
busClient.unsubscribe(tag)
busClient.publish(to, data)   // data is a JS object
busClient.launch(name)
busClient.injectQml(target, name, url, source)

// Signals
busClient.onMessageReceived(tag, sender, data)
busClient.onInjectionRequested(name, url, source)
busClient.onConnectedChanged()
busClient.onErrorOccurred(error)
```

`publish("sensor", {value: 1})` broadcasts to all `"sensor"` subscribers.
`publish("sensor:DisplayApp", {value: 1})` unicasts to `DisplayApp` only.

---

## Patterns

### Pattern 1 — Direct pub/sub (raw protocol)

The simplest pattern. Publisher and subscriber agree on a tag name and a JSON
schema by convention.

**Publisher:**
```qml
// SensorApp/App.qml
Timer {
    interval: 1000; running: true; repeat: true
    onTriggered: busClient.publish("sensor", { value: counter++, unit: "°C" })
}
```

**Subscriber:**
```qml
// DisplayApp/App.qml
Text {
    id: reading
    Connections {
        target: busClient
        function onMessageReceived(tag, sender, data) {
            if (tag === "sensor") reading.text = data.value + " " + data.unit
        }
    }
}
```

---

### Pattern 2 — Property mirroring (proxy/foreign in plain QML)

You can replicate the proxy/foreign pattern entirely in QML using `busClient`
publish and subscribe — no extra C++ required.

**Convention:** agree on a tag and a JSON schema. Use two message shapes:
- `{"properties": {"key": value, ...}}` — state snapshot or delta
- `{"action": "signalName", "args": [...]}` — signal/event notification

**Publisher side (process A — SensorApp/App.qml):**
```qml
import QtQuick 2.15

Item {
    property int  temperature: 0
    property bool heaterOn: false

    // Publish full snapshot on connect, then deltas on change
    Connections {
        target: busClient
        function onConnectedChanged() {
            if (busClient.connected)
                busClient.publish("status", { properties: { temperature: temperature, heaterOn: heaterOn } })
        }
    }

    onTemperatureChanged: busClient.publish("status", { properties: { temperature: temperature } })
    onHeaterOnChanged:    busClient.publish("status", { properties: { heaterOn: heaterOn } })

    function triggerAlarm(reason) {
        busClient.publish("status", { action: "alarmTriggered", args: [reason] })
    }

    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: {
            temperature++
            if (temperature > 80) parent.triggerAlarm("overheat")
        }
    }
}
```

**Subscriber side (process B — DisplayApp/App.qml):**
```qml
import QtQuick 2.15

Item {
    property int  temperature: 0
    property bool heaterOn: false

    Component.onCompleted: busClient.subscribe("status")

    Connections {
        target: busClient
        function onMessageReceived(tag, sender, data) {
            if (tag !== "status") return
            if (data.properties) {
                if (data.properties.temperature !== undefined) temperature = data.properties.temperature
                if (data.properties.heaterOn    !== undefined) heaterOn    = data.properties.heaterOn
            }
            if (data.action === "alarmTriggered")
                console.log("ALARM:", data.args[0])
        }
    }

    onTemperatureChanged: console.log("temp:", temperature)
}
```

**Wire messages for this example:**

| Event | JSON on bus |
|-------|-------------|
| SensorApp connects | `{"properties":{"temperature":0,"heaterOn":false}}` |
| `temperature` changes to 23 | `{"properties":{"temperature":23}}` |
| `heaterOn` changes to true | `{"properties":{"heaterOn":true}}` |
| alarm fires | `{"action":"alarmTriggered","args":["overheat"]}` |

**Unicast to a specific receiver:**
```qml
busClient.publish("status:DisplayApp", { properties: { temperature: temperature } })
```

This is the full pattern. You own the schema and can extend it with any fields
(timestamps, sequence numbers, error codes) without any framework changes.

---

### Pattern 3 — QML injection

Used when you want to push UI components into a running window on demand
rather than starting a new window per feature.

**Host app (`ShellApp/App.qml`):**
```qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

ApplicationWindow {
    visible: true; width: 800; height: 600

    Connections {
        target: busClient
        function onInjectionRequested(name, url, source) {
            var comp = url ? Qt.createComponent(url)
                           : Qt.createQmlObject(source, panels, name)
            if (!comp || !comp.status) return     // createQmlObject path done
            if (comp.status === Component.Ready) {
                comp.createObject(panels)
            } else if (comp.status === Component.Loading) {
                comp.statusChanged.connect(function() {
                    if (comp.status === Component.Ready) comp.createObject(panels)
                })
            }
        }
    }

    Column { id: panels; anchors.fill: parent; spacing: 8 }
}
```

**Panel app (`SensorPanel/Launch.qml`):**
```qml
import ConcertoBus 1.0
LaunchSpec {
    name:      "SensorPanel"
    transport: "stdio"
    attachTo:  "ShellApp"    // ← inject, don't spawn
    mainQml:   "App.qml"
}
```

**Panel app (`SensorPanel/App.qml`) — runs inside ShellApp's engine:**
```qml
import QtQuick 2.15
Rectangle {
    width: parent ? parent.width : 400; height: 72
    color: "#1e1e2e"; radius: 6

    property int value: 0
    Component.onCompleted: busClient.subscribe("sensor")

    Connections {
        target: busClient
        function onMessageReceived(tag, sender, data) {
            if (tag === "sensor") value = data.value
        }
    }

    Text { anchors.centerIn: parent; text: "Sensor: " + value; color: "white"; font.pixelSize: 24 }
}
```

Note: the injected component runs **inside ShellApp's QML engine** and shares
ShellApp's `busClient`. `busClient.subscribe("sensor")` in the panel therefore
subscribes ShellApp to the `"sensor"` tag.

**Triggering injection:**

From `config.qml` / daemon: add the panel as an `App` entry, then call
`busClient.launch("SensorPanel")` from any connected client.

Programmatically from any QML app:
```qml
// by file URL
busClient.injectQml("ShellApp", "MyPanel", "file:///D:/apps/Panel.qml", "")

// by inline source
busClient.injectQml("ShellApp", "Counter", "",
    "import QtQuick 2.0; Text { text: 'Hello'; color: 'white' }")
```

---

## Building a custom protocol

Follow this checklist for any new publish/subscribe protocol:

### 1. Choose a tag name

Pick a short, lowercase, dot-separated name: `"sensor"`, `"alarm.fire"`,
`"ui.button.pressed"`.

### 2. Define the JSON schema

Document what keys your messages carry. Example:

```
tag: "alarm"

Publisher sends:
  {"level":"warning","code":42,"message":"High temperature"}

Subscriber expects:
  data.level   — "info" | "warning" | "critical"
  data.code    — integer
  data.message — string
```

No schema enforcement happens in the daemon. Agree on it in comments or a
separate doc.

### 3. Subscribe in Launch.qml or at runtime

```qml
// Static — subscribed before App.qml loads
LaunchSpec { subscribes: ["alarm"] }

// Dynamic — subscribed when the component needs it
Component.onCompleted: busClient.subscribe("alarm")
```

### 4. Publish

```qml
busClient.publish("alarm", { level: "warning", code: 42, message: "High temp" })
```

### 5. Receive

```qml
Connections {
    target: busClient
    function onMessageReceived(tag, sender, data) {
        if (tag !== "alarm") return
        if (data.level === "critical") shutdownSequence()
    }
}
```

### 6. Unicast vs broadcast

```qml
busClient.publish("alarm", data)               // all "alarm" subscribers
busClient.publish("alarm:SafetyController", data)  // only SafetyController
```

Offline queue: if the target isn't online yet, the daemon queues the message
and delivers it when the subscriber connects. This guarantees delivery for the
first message from a fast producer.

---

## `config.qml` reference

```qml
import QtQml 2.0
import ConcertoBusConfig 1.0

BusConfig {
    // Optional TCP transport for external clients
    transports: [
        Transport { plugin: "tcp"; port: 49152 }
    ]

    // Optional XMPP gateway for cross-machine federation
    gateways: [
        Gateway {
            plugin:   "xmpp"
            user:     "runner-a@xmpp.example.com"
            password: "secret"
            peers:    ["runner-b@xmpp.example.com"]
        }
    ]

    // Each App {} entry points to a directory containing Launch.qml.
    // Paths are relative to config.qml.
    App { id: sns; path: "SensorApp" }
    App { id: dsp; path: "DisplayApp" }
    App { id: shell; path: "ShellApp" }
    App { id: panel; path: "SensorPanel" }  // attachTo → inject-style, no new window

    Component.onCompleted: {
        sns.start()     // auto-launch
        dsp.start()
        shell.start()
        // panel is launched on demand via busClient.launch("SensorPanel")
    }
}
```

`app.start()` sets `autoLaunch = true`. The daemon launches those processes
immediately after loading the config.

---

## Running the demos

### stdio demo (inject pattern)

```powershell
# From repo root
build\RelWithDebInfo\pm.exe -c demo\config.qml
# ShellApp window appears. Click "Load Sensor Panel" to inject the counter widget.
```

### XMPP demo (cross-machine federation)

```powershell
# Machine A
build\RelWithDebInfo\pm.exe -c demo\xmpp-demo\runner-a\config.qml

# Machine B
build\RelWithDebInfo\pm.exe -c demo\xmpp-demo\runner-b\config.qml
```

---

## Troubleshooting

| Symptom | Likely cause |
|---------|-------------|
| `{"error":"not_registered"}` | Sent a command before `register` completed |
| `{"error":"name_taken"}` | Another client already used this name |
| `{"error":"unknown_process"}` | `launch` name not in config or `exe` missing and `attachTo` empty |
| `{"error":"target_not_found"}` | `inject` target not registered — it hasn't started yet |
| Message not received | Check tag spelling; check subscriber is online before publish or relies on queue drain |
| `Cannot create window: no screens` | `client.exe` built with `QCoreApplication` — needs `QGuiApplication` |
| `Property 'launch' is not a function` | Method missing from `AbstractBusClient` interface — add to base class and all implementations |
