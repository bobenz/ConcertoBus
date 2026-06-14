# ConcertoBus Developer Guide

ConcertoBus is a local message bus for Qt applications. Processes connect over
TCP or stdin/stdout, register a name, subscribe to topics, and exchange JSON
messages. A central daemon (`pm.exe`) manages process lifecycles. Apps describe
themselves in a `Launch.qml` file; a generic host binary (`client.exe`) reads
that file and drives everything.

---

## Table of Contents

1. [Architecture overview](#1-architecture-overview)
2. [Wire protocol](#2-wire-protocol)
3. [Writing an app — QML](#3-writing-an-app--qml)
4. [Writing an app — C++](#4-writing-an-app--c)
5. [Designing a protocol on top of the bus](#5-designing-a-protocol-on-top-of-the-bus)
6. [Daemon configuration](#6-daemon-configuration)
7. [Process lifecycle and launch-on-demand](#7-process-lifecycle-and-launch-on-demand)
8. [Building and running](#8-building-and-running)
9. [Reference — wire messages](#9-reference--wire-messages)

---

## 1. Architecture overview

```
┌─────────────────────────────────────────────┐
│  pm.exe  (daemon)                           │
│  ┌──────────┐  ┌───────────┐  ┌─────────┐  │
│  │BusCore   │  │ProcessMgr │  │ Router  │  │
│  │          │◄─┤           │  │         │  │
│  └────┬─────┘  └───────────┘  └────┬────┘  │
│       │  TCP port 49152            │       │
└───────┼────────────────────────────┼───────┘
        │                            │ (internal)
   ┌────▼────┐   stdio pipes    ┌────▼────┐
   │TCP      │                  │Stdio    │
   │clients  │                  │children │
   └─────────┘                  └─────────┘
  BusClient (TCP)            StdioBusClient
  (external processes,       (processes launched
   GUI tools, controllers)    by pm.exe itself)
```

**Key concepts:**

| Concept | Meaning |
|---------|---------|
| **Name** | Unique string a process registers with (`"Sensor"`, `"Display"`) |
| **Tag** | Topic string used for publish/subscribe (`"sensor"`, `"control"`) |
| **Transport** | How a process connects: `"stdio"` (launched by pm) or `"tcp"` (connects itself) |
| **Launch.qml** | Self-describing app descriptor — read by both pm and `client.exe` |

A message published to tag `"sensor"` is delivered to every process subscribed
to `"sensor"`. A unicast to `"sensor:Display"` goes only to the process named
`"Display"` if it is subscribed to `"sensor"`.

---

## 2. Wire protocol

All messages are newline-delimited JSON. Each command is one JSON object per
line. Every client must send `register` as its very first message; all other
commands are rejected until registration succeeds.

### Client → daemon

```jsonc
// Mandatory first message
{"cmd":"register","name":"MyApp"}

// Subscribe to a tag
{"cmd":"subscribe","tag":"sensor"}

// Unsubscribe
{"cmd":"unsubscribe","tag":"sensor"}

// Publish to all subscribers of "sensor"
{"cmd":"publish","to":"sensor","data":{"value":42}}

// Publish unicast — only reaches "Display" if it subscribed to "sensor"
{"cmd":"publish","to":"sensor:Display","data":{"value":42}}

// Ask pm to start a managed process (TCP clients only)
{"cmd":"launch","name":"Display"}
```

### Daemon → client

```jsonc
// Register accepted
{"ok":true}

// Register rejected (name already taken)
{"error":"name_taken"}

// Incoming message delivered to a subscriber
{"push":true,"from":"sensor","sender":"Sensor","data":{"value":42}}

// Launch acknowledged — process is starting
{"event":"launching","name":"Display"}

// Process is now registered and reachable
{"event":"process_started","name":"Display"}

// Process exited cleanly
{"event":"process_stopped","name":"Display"}

// Process crashed
{"event":"process_crashed","name":"Display"}
```

### Offline queuing

If a message is published before the target subscriber exists, the router
queues it keyed by `"tag:name"` or `"tag:*"`. When the subscriber registers
and subscribes to that tag, the queue is drained automatically. This means
producers can publish before consumers are ready.

---

## 3. Writing an app — QML

Every app lives in its own directory and contains two files:
`Launch.qml` (descriptor) and `App.qml` (logic/UI).

### 3.1 Launch.qml

```qml
import ConcertoBus 1.0

LaunchSpec {
    name:      "Sensor"         // unique bus name
    transport: "stdio"          // "stdio" (default) or "tcp"
    subscribes: ["control"]     // tags to auto-subscribe on connect
    mainQml:   "App.qml"        // entry point relative to this file

    // Called after App.qml is loaded and the bus connection is open
    onCompleted: console.log("[Sensor] ready")
}
```

`transport: "stdio"` means pm launches this app as a child process and
communicates via pipes — no network port needed.

`transport: "tcp"` means the app connects itself to `127.0.0.1:49152`.
Use this for external tools, GUI front-ends, or processes not managed by pm.

### 3.2 App.qml — receiving messages

The host exposes a context property `busClient` of type `AbstractBusClient`.
Use a `Connections` object to handle its signals:

```qml
import QtQml

QtObject {
    property Connections busConn: Connections {
        target: busClient

        // Fired once after registration is confirmed
        function onConnectedChanged() {
            if (busClient.connected)
                console.log("registered as:", busClient.name)
        }

        // tag   — the topic the message was published to
        // sender — the name of the publishing process
        // data   — the JSON payload as a JS object
        function onMessageReceived(tag, sender, data) {
            console.log("got", tag, "from", sender, JSON.stringify(data))
        }
    }
}
```

### 3.3 App.qml — publishing messages

```qml
// Broadcast: all subscribers of "sensor" receive this
busClient.publish("sensor", { value: 42, unit: "°C" })

// Unicast: only "Display" receives this (if it subscribed to "sensor")
busClient.publish("sensor:Display", { value: 42 })
```

### 3.4 App.qml — subscribing at runtime

Tags listed in `Launch.qml`'s `subscribes` are applied automatically. To
subscribe to additional tags at runtime:

```qml
busClient.subscribe("alerts")
busClient.unsubscribe("alerts")
```

### 3.5 Full example — SensorApp

**`SensorApp/Launch.qml`**
```qml
import ConcertoBus 1.0

LaunchSpec {
    name:       "Sensor"
    transport:  "stdio"
    subscribes: ["control"]
    mainQml:    "App.qml"
}
```

**`SensorApp/App.qml`**
```qml
import QtQml

QtObject {
    property int value: 0

    property Timer tick: Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: {
            value++
            busClient.publish("sensor", { value: value, source: busClient.name })
        }
    }

    property Connections ctrl: Connections {
        target: busClient
        function onMessageReceived(tag, sender, data) {
            if (tag === "control" && data.cmd === "reset")
                value = 0
        }
    }
}
```

---

## 4. Writing an app — C++

Use `AbstractBusClient` when you need type-safe access, integration with
existing C++ code, or a non-QML process.

### 4.1 Stdio app (launched by pm)

```cpp
#include <StdioBusClient.h>
#include <QCoreApplication>
#include <QJsonObject>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    StdioBusClient client;
    client.setName("SensorCpp");

    QObject::connect(&client, &AbstractBusClient::connectedChanged, [&]() {
        if (client.isConnected()) {
            client.subscribe("control");
        }
    });

    QObject::connect(&client, &AbstractBusClient::messageReceived,
        [](const QString &tag, const QString &sender, const QJsonObject &data) {
            qInfo() << "got" << tag << "from" << sender << data;
        });

    QTimer ticker;
    ticker.setInterval(1000);
    int n = 0;
    QObject::connect(&ticker, &QTimer::timeout, [&]() {
        client.publish("sensor", QJsonObject{{"value", ++n}});
    });

    QObject::connect(&client, &AbstractBusClient::connectedChanged, [&]() {
        if (client.isConnected()) ticker.start();
    });

    client.connectToBus();
    return app.exec();
}
```

Link against `ConcertoBusClient` and `Qt6::Core Qt6::Network Qt6::Qml`.

### 4.2 TCP app (connects externally)

```cpp
#include <BusClient.h>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    BusClient client;
    client.setName("ControllerCpp");
    client.setHost("127.0.0.1");
    client.setPort(49152);

    QObject::connect(&client, &AbstractBusClient::connectedChanged, [&]() {
        if (client.isConnected()) {
            client.subscribe("sensor");
            client.launch("Display");   // ask pm to start Display
        }
    });

    QObject::connect(&client, &BusClient::launchReady, [&](const QString &name) {
        qInfo() << name << "is up — starting to send data";
        client.publish("sensor", QJsonObject{{"value", 1}});
    });

    QObject::connect(&client, &AbstractBusClient::messageReceived,
        [](const QString &tag, const QString &sender, const QJsonObject &data) {
            qInfo() << tag << sender << data;
        });

    client.connectToBus();
    return app.exec();
}
```

### 4.3 Subclassing AbstractBusClient

If you want a custom transport (e.g. WebSocket, shared memory), subclass
`AbstractBusClient` and implement the pure virtual methods. The rest of the
bus protocol is handled by the daemon; your subclass only needs to move bytes.

```cpp
class WebSocketBusClient : public AbstractBusClient {
    Q_OBJECT
public:
    QString name() const override { return m_name; }
    void setName(const QString &n) override { m_name = n; emit nameChanged(); }
    bool isConnected() const override { return m_connected; }

    void connectToBus() override { /* open WS, send register */ }
    void subscribe(const QString &tag) override { sendJson({{"cmd","subscribe"},{"tag",tag}}); }
    void unsubscribe(const QString &tag) override { sendJson({{"cmd","unsubscribe"},{"tag",tag}}); }
    void publish(const QString &to, const QJsonObject &data) override {
        sendJson({{"cmd","publish"},{"to",to},{"data",data}});
    }
private:
    void sendJson(const QJsonObject &obj) { /* write JSON line to WS */ }
    // on incoming line: parse push/ok/event and emit the right signal
    QString m_name;
    bool m_connected = false;
};
```

---

## 5. Designing a protocol on top of the bus

The `data` field in every publish message is an arbitrary JSON object. You
define your own schema there. The bus only cares about `"to"` and `"from"`;
it forwards `"data"` verbatim.

### 5.1 Define your topics and message shapes

Pick a topic name per data stream. Keep topics coarse (one per logical data
source), not fine-grained (one per field). Document the shape as a simple
type:

```
Topic: "sensor"
Publisher: Sensor
Schema: { "value": number, "unit": string, "source": string }

Topic: "control"
Publisher: any controller app
Schema: { "cmd": "reset" | "calibrate", "param"?: number }

Topic: "alert"
Publisher: any process
Schema: { "level": "info" | "warn" | "error", "msg": string }
```

### 5.2 Broadcast vs unicast

| Intent | `to` field | When to use |
|--------|-----------|-------------|
| Broadcast | `"sensor"` | Any subscriber may act — logging, display, storage |
| Unicast | `"sensor:Display"` | Only one specific process should receive the message |
| Reply | `"reply:Controller"` (convention) | Response directed back to the requester |

Unicast is delivery-conditional: if `Display` is not subscribed to `"sensor"`,
the message is queued, not dropped. The queue is keyed by `"sensor:Display"`.

### 5.3 Request / response pattern

Because the bus is asynchronous, request/response requires a correlation ID
and a dedicated reply topic. Convention:

```jsonc
// Requester publishes (unicast to the service):
{"cmd":"publish","to":"rpc:Calculator",
 "data":{"id":"req-1","method":"add","a":3,"b":4}}

// Calculator publishes the reply (unicast back):
{"cmd":"publish","to":"rpc:Controller",
 "data":{"id":"req-1","result":7}}
```

QML implementation:

```qml
// Controller/App.qml
property int _nextId: 0

function call(method, params, callback) {
    var id = "req-" + (++_nextId)
    _pending[id] = callback
    busClient.publish("rpc:Calculator", Object.assign({ id: id, method: method }, params))
}

property var _pending: ({})

Connections {
    target: busClient
    function onMessageReceived(tag, sender, data) {
        if (tag === "rpc" && _pending[data.id]) {
            _pending[data.id](data)
            delete _pending[data.id]
        }
    }
}

// Usage:
Component.onCompleted: {
    busClient.subscribe("rpc")
    call("add", {a:3, b:4}, function(r){ console.log("result:", r.result) })
}
```

### 5.4 State synchronisation pattern

When a new subscriber joins it may have missed earlier publications. Use the
offline-queue: the publisher sends periodic full-state snapshots, or sends one
on any subscribe event from a topic watcher. Simpler approach — publish on
every change, rely on the offline queue:

```qml
// Producer: publish on every change, bus queues for late subscribers
onValueChanged: busClient.publish("state:Sensor", { value: value, ts: Date.now() })
```

The bus queues `"state:Sensor"` until a client subscribes to `"state"` with
name `"Sensor"` in scope, then drains immediately. The subscriber always gets
the latest snapshot on join.

### 5.5 Versioning

Add a `"v"` field to your schema from the start:

```jsonc
{"v":1,"value":42,"unit":"°C"}
```

Consumers check `data.v` and ignore or adapt messages from unknown versions.
This lets you evolve the schema without a coordinated restart.

---

## 6. Daemon configuration

The daemon reads a `config.qml` file using `import ConcertoBusConfig 1.0`.

### 6.1 Minimal config — auto-launch two stdio apps

```qml
import QtQml
import ConcertoBusConfig 1.0

BusConfig {
    App { id: sensor;  path: "SensorApp" }
    App { id: display; path: "DisplayApp" }

    Component.onCompleted: {
        sensor.start()
        display.start()
    }
}
```

`path` is the directory containing `Launch.qml`, relative to `config.qml`.
`start()` marks the app for auto-launch at daemon startup.

### 6.2 Config — on-demand launch (TCP controller)

Omit `Component.onCompleted`. Apps not marked `start()` are registered in the
process table but not started. A TCP client can later request launch:

```qml
import ConcertoBusConfig 1.0

BusConfig {
    App { path: "DisplayApp" }   // registered but not auto-launched
}
```

### 6.3 Config — TCP transport

The daemon always listens on TCP port 49152. There is no extra config needed;
any process can connect to that port.

### 6.4 App directory layout

```
MyApp/
  Launch.qml    ← descriptor (read by pm and client.exe)
  App.qml       ← QML entry point (or Main.qml, set via mainQml:)
  assets/       ← anything else your app needs
```

---

## 7. Process lifecycle and launch-on-demand

### 7.1 Auto-launch at startup

Mark apps with `app.start()` in `Component.onCompleted`. pm launches them
immediately, attaches their stdin/stdout as a stdio transport channel, and they
register on the bus automatically (handled by `StdioBusClient.connectToBus()`).

### 7.2 On-demand launch via TCP

A TCP client calls:

```qml
busClient.launch("Display")
```

pm receives `{"cmd":"launch","name":"Display"}`, starts `client.exe
path/to/DisplayApp/Launch.qml`, and notifies the requester:

1. `{"event":"launching","name":"Display"}` — start acknowledged
2. `{"event":"process_started","name":"Display"}` — Display registered on bus

The requester becomes a **watcher**: it receives `process_stopped` and
`process_crashed` events for Display for the lifetime of the TCP connection.

BusClient signals in QML:
```qml
function onLaunchReady(name)          { /* process_started */ }
function onLaunchError(name, reason)  { /* process_stopped / process_crashed */ }
```

### 7.3 Auto-restart

Set `autoRestart: true` on a `Process` entry (advanced config, bypassing
Launch.qml — direct `ProcessManager::addEntry()` from C++):

```cpp
pm.addEntry("Sensor", clientExe, {launchQmlPath},
            workDir, {"control"}, "stdio",
            /*autoRestart=*/true, /*delayMs=*/2000, /*maxRestarts=*/5);
```

The process manager restarts the process up to `maxRestarts` times after a
crash, waiting `restartDelayMs` between attempts.

---

## 8. Building and running

### 8.1 Build (CMake)

```powershell
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH="D:/Qt/6.10.3/msvc2022_64"
cmake --build build --config RelWithDebInfo
```

Outputs in `build\RelWithDebInfo\`: `pm.exe`, `client.exe`.

### 8.2 Build (qmake / Qt Creator)

Open `ConcertoBus.pro` in Qt Creator. Select a Qt 6 MSVC 2022 64-bit kit.
Build → Build All.

### 8.3 Run demo 1 — stdio auto-launch

```powershell
build\RelWithDebInfo\pm.exe -c demo\config.qml
```

pm launches Sensor and Display automatically. Both register on the bus via
stdio. Sensor publishes a counter every second; Display prints it.

### 8.4 Run demo 2 — TCP controller + on-demand launch

```powershell
# Terminal 1 — daemon (knows about Display but doesn't start it)
build\RelWithDebInfo\pm.exe -c demo\tcp-demo\config.qml

# Terminal 2 — TCP controller (connects, requests Display, then publishes)
build\RelWithDebInfo\client.exe demo\ControllerApp\Launch.qml
```

Controller connects via TCP, sends `launch Display`, waits for
`process_started`, then publishes sensor data every second. Display receives
it via stdio.

### 8.5 Run tests

```powershell
cd build
ctest -C RelWithDebInfo --output-on-failure
```

---

## 9. Reference — wire messages

### 9.1 Client → daemon

| Message | Required fields | Description |
|---------|----------------|-------------|
| `register` | `name` | First message. Claim a bus name. |
| `subscribe` | `tag` | Begin receiving publishes to this tag. |
| `unsubscribe` | `tag` | Stop receiving publishes to this tag. |
| `publish` | `to`, `data` | Send data. `to` = tag or `tag:name`. |
| `launch` | `name` | Start a managed process (TCP only). |
| `kill` | `name` | Stop a managed process (TCP only). |
| `restart` | `name` | Kill and relaunch a managed process (TCP only). |

### 9.2 Daemon → client

| Message | Fields | Description |
|---------|--------|-------------|
| ok ack | `ok: true` | `register` succeeded. |
| error | `error`, `name?` | Something failed. |
| push | `push: true`, `from`, `sender`, `data` | Incoming message. |
| launching | `event: "launching"`, `name` | Process start acknowledged. |
| process_started | `event: "process_started"`, `name` | Process registered on bus. |
| process_stopped | `event: "process_stopped"`, `name` | Process exited cleanly. |
| process_crashed | `event: "process_crashed"`, `name` | Process exited with error. |

### 9.3 Push message fields

```jsonc
{
  "push":   true,
  "from":   "sensor",     // the tag the message was published to
  "sender": "Sensor",     // the registered name of the publisher
  "data":   { ... }       // arbitrary payload defined by your protocol
}
```

In QML / C++ the `messageReceived(tag, sender, data)` signal maps directly
to `from`, `sender`, `data`.

### 9.4 Addressing summary

| `to` value | Delivered to |
|-----------|-------------|
| `"sensor"` | All processes subscribed to `"sensor"` |
| `"sensor:Display"` | Only `Display`, if it is subscribed to `"sensor"` |

Messages to unknown targets (no subscribers, not in queue key) are silently
queued under the `"tag:name"` or `"tag:*"` key and delivered when the
subscriber eventually connects.
