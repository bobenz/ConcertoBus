# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires Qt 6 (Core, Network, Qml, Quick) and MSVC 2022 on Windows. Qt is expected at `D:\Qt\6.10.3\msvc2022_64`.

```powershell
# Standard build (no optional plugins)
cmake -B build -DCMAKE_PREFIX_PATH="D:/Qt/6.10.3/msvc2022_64"
cmake --build build --config RelWithDebInfo

# With XMPP federation gateway (QXmpp at D:\Qt\qxmpp-installed)
cmake -B build -DCONCERTO_BUS_XMPP=ON -DCMAKE_PREFIX_PATH="D:/Qt/6.10.3/msvc2022_64"
cmake --build build --config RelWithDebInfo
```

CMake build options:
- `CONCERTO_BUS_XMPP=ON` — build the XMPP gateway plugin (requires QXmpp at `D:\Qt\qxmpp-installed`)
- `CONCERTO_BUS_WS=ON` — build the WebSocket transport plugin (placeholder, not yet implemented)

## Tests

```powershell
cd build
ctest -C RelWithDebInfo --output-on-failure

# Run a single test binary directly
.\RelWithDebInfo\tst_config.exe
.\RelWithDebInfo\tst_processmanager.exe
.\RelWithDebInfo\tst_router.exe
.\RelWithDebInfo\tst_stdio.exe
.\RelWithDebInfo\tst_buscore.exe
.\RelWithDebInfo\tst_qt5client.exe
```

## Deploy (Windows)

```powershell
.\deploy\Deploy-ConcertoBus.ps1                    # XMPP build, defaults
.\deploy\Deploy-ConcertoBus.ps1 -WithXmpp:$false   # no-XMPP build
```

Output lands in `dist\ConcertoBus-1.0-Win64\`. See script header for all parameters.

## Running the demos

```powershell
# Stdio + QML injection demo (default demo)
build\RelWithDebInfo\pm.exe -c demo\config.qml

# XMPP cross-machine federation demo (two terminals / two machines)
build\RelWithDebInfo\pm.exe -c demo\xmpp-demo\runner-a\config.qml
build\RelWithDebInfo\pm.exe -c demo\xmpp-demo\runner-b\config.qml
```

## Security note

`demo\xmpp-demo\runner-a\config.qml` and `demo\xmpp-demo\runner-b\config.qml` contain
plain-text XMPP passwords. The credential classifier blocks automated commits for these
files. **Never commit them via automated tools — the user must commit them manually.**

---

## Architecture

ConcertoBus is a language-agnostic message bus and process manager. The wire protocol is
newline-delimited JSON over stdio pipes or TCP. **Any process that can read/write JSON lines
is a participant** — Qt/QML, Python, Node.js, shell scripts via `nc`, etc.

### Directory structure

```
bus/                    Daemon core
  main.cpp              Entry point: parses config, loads plugins, starts BusCore
  BusCore.h/.cpp        Central wiring: IBusTransport* list + Router + ProcessManager
  Router.h/.cpp         Pure routing: registry, subscriptions, offline queues
  ProcessManager.h/.cpp QProcess lifecycle: launch, kill, restart, autoRestart
  IBusTransport.h/.cpp  Plugin interface + ClientId typedef (quintptr)
  IBusGateway.h/.cpp    Plugin interface for federation gateways
  BusServer.h/.cpp      (legacy, unused — superseded by BusCore)
  Catalog.h/.cpp        (legacy, unused — superseded by ProcessManager)
  config/
    BusConfigTypes.h    QObject types: BusConfig, AppDef, TransportDef, GatewayDef, ProcessDef
    BusConfigLoader.h   Scans App{} entries in config.qml, builds ProcessDef list
    LaunchSpecReader.h  Minimal LaunchSpec reader used by the daemon (no QML engine needed)

transports/
  stdio/StdioTransport.h/.cpp   Built-in, statically linked. Wraps QProcess stdin/stdout.
  tcp/TcpTransport.h/.cpp       Qt plugin (MODULE). Loaded at runtime via QPluginLoader.
                                 Listens on a TCP port; each socket is a ClientId.

gateways/
  xmpp/XmppGateway.h/.cpp      Qt plugin (MODULE). XMPP federation between runners.
                                 Loaded from <exe>/gateways/XmppGateway.dll at runtime.

client/
  AbstractBusClient.h/.cpp      Abstract base: name, connected, subscribe, publish, etc.
  BusClient.h/.cpp              TCP implementation (QTcpSocket). QML_ELEMENT.
  StdioBusClient.h/.cpp         Stdio implementation. QML_ELEMENT. Used inside client.exe.
  LaunchSpec.h/.cpp             QML type describing an app (name, transport, attachTo, …).
  ConcertoBusPlugin.cpp         QML plugin registration.
  host/main.cpp                 Generic QML host (client.exe): loads Launch.qml, creates
                                 the right AbstractBusClient, loads App.qml.

demo/
  config.qml                    Default demo: SensorApp + ShellApp + SensorPanel (inject)
  SensorApp/                    Publishes a counter every second
  ShellApp/                     Window host; button injects SensorPanel on demand
  SensorPanel/                  Injected widget (attachTo: "ShellApp")
  DisplayApp/                   Simple subscriber display
  ControllerApp/                Sends control messages
  Qt5ClientApp/                 Raw TCP client (no ConcertoBus lib, works from Qt 5)
  tcp-demo/                     TCP transport demo config
  xmpp-demo/                    Two-runner XMPP federation demo

tests/
  tst_config.cpp                BusConfigLoader parsing
  tst_processmanager.cpp        ProcessManager launch/stop/autoRestart
  tst_router.cpp                Router: register, subscribe, publish, unicast, queue drain
  tst_stdio.cpp                 End-to-end stdio: daemon launches stdio_echo_helper
  tst_buscore.cpp               BusCore: launch/kill/restart watcher notifications
  tst_qt5client.cpp             Raw TCP client interop via TcpTransport plugin
  stdio_echo_helper/            Minimal StdioBusClient app used by tst_stdio

docs/
  developer-guide.md            Full protocol reference + patterns (canonical doc)
  ConcertoBus-Guide.md          Older guide (may be stale)
```

---

### Daemon (`bus/`)

**`BusCore`** — central coordinator.
- Holds one built-in `StdioTransport` (always present) plus any runtime-loaded `IBusTransport` plugins (e.g. `TcpTransport`).
- Holds the `Router` and optionally a `ProcessManager`.
- On every incoming JSON line from any transport: dispatches `register`, `subscribe`, `unsubscribe`, `publish`, `launch`, `kill`, `restart`, `inject` commands.
- Tracks **process watchers**: clients that issued `launch`/`kill`/`restart` receive async push events (`process_started`, `process_stopped`, `process_crashed`, `injected`).
- On `inject` command: looks up target's `ClientId` via `Router::idFor(name)`, pushes inject payload directly to that client.
- Calls gateway hooks (`onLocalRegister`, `onLocalSubscribe`, `onLocalPublish`, …) for every loaded `IBusGateway`.

**`Router`** — pure routing, no I/O.
- `ClientId` = `quintptr` (the transport identifies each connection by its pointer cast to quintptr).
- `m_registry`: `QHash<QString, ClientId>` — name → id.
- `m_tagSubscriptions`: `QHash<QString, QSet<ClientId>>` — tag → subscriber set.
- `m_queues`: offline message queue; drained when a subscriber connects.
- Publish addressing: `"tagname"` broadcasts; `"tagname:TargetName"` unicasts.
- Emits `published(tag, sender, data)` signal — connected to `IBusGateway::onLocalPublish`.

**`ProcessManager`** — QProcess lifecycle.
- `Entry` struct: exe, args, workingDir, subscribes, transport, attachTo, mainQmlUrl, autoLaunch, autoRestart, restartCount, maxRestarts.
- `attachTo` non-empty → inject-style app; no QProcess is spawned on `launch`.
- Guard in `load()`: skip entries where both `exe` and `attachTo` are empty.
- Signals: `processStarted(name)`, `processStopped(name)`, `processCrashed(name)`.

**`IBusTransport`** interface — `ClientId = quintptr`.
- Signals: `clientConnected(ClientId)`, `clientDisconnected(ClientId)`, `messageReceived(ClientId, QByteArray)`.
- Methods: `send(ClientId, QByteArray)`, `closeClient(ClientId)`.
- `StdioTransport` is built-in (static). `TcpTransport` is a Qt MODULE plugin loaded via `QPluginLoader`.

**`IBusGateway`** interface — federation.
- `start(QVariantMap config)` — called once after loading.
- Hooks: `onLocalRegister/Unregister`, `onLocalSubscribe/Unsubscribe`, `onLocalPublish`.
- Signal: `remotePublished(tag, sender, data)` — injected into local Router.
- `XmppGateway`: each runner is an XMPP client; local processes appear as XMPP presence resources; subscriptions and publishes are forwarded as `<message>` stanzas to peer runners.

**Config loading** (`bus/config/`):
- `BusConfig` / `App` / `Transport` / `Gateway` are small `QObject` types registered under `ConcertoBusConfig 1.0`.
- `BusConfigLoader` scans each `App { path: "..." }` for a `Launch.qml`, reads it via a minimal `LaunchSpecReader` (not the full QML plugin), and builds `ProcessDef` entries.
- `LaunchSpecReader` mirrors `LaunchSpec` properties so the daemon can parse `Launch.qml` without importing `ConcertoBus 1.0`.

---

### Client library (`client/`)

- **`AbstractBusClient`** — abstract `QObject` base. Properties: `name`, `connected`. Methods: `connectToBus()`, `subscribe(tag)`, `unsubscribe(tag)`, `publish(to, data)`, `launch(name)`, `injectQml(target, name, url, source)`. Signals: `messageReceived(tag, sender, data)`, `injectionRequested(name, url, source)`, `connectedChanged`, `errorOccurred`.
- **`BusClient`** — TCP implementation (`QTcpSocket`). `QML_ELEMENT`.
- **`StdioBusClient`** — stdio implementation. Reads stdin on a background `QThread`. Writes stdout. `QML_ELEMENT`. Used by `client.exe` and by `Qt.createQmlObject` injected panels.
- **`LaunchSpec`** — `QML_ELEMENT`. Properties: `name`, `transport` (`"stdio"`/`"tcp"`), `subscribes`, `importPaths`, `mainQml`, `attachTo`. When `attachTo` is non-empty, `client.exe` sends an `inject` command and exits instead of loading an engine.
- **`ConcertoBusPlugin.cpp`** — registers all QML_ELEMENTs under URI `ConcertoBus`, version `2.0`.

**`client.exe` startup sequence** (`client/host/main.cpp`):
1. Load `Launch.qml` → get `LaunchSpec*`.
2. If `attachTo` is set → register as `"<name>_launcher"`, send inject, quit after 500 ms.
3. Otherwise: create `StdioBusClient` or `BusClient` based on `transport`.
4. Set name, `connectToBus()`, subscribe to `spec->subscribes()`.
5. Set `busClient` as QML context property.
6. Add import paths from `spec->importPaths()`.
7. Load `spec->mainQml()` (resolved relative to the Launch.qml directory).
8. Emit `spec->completed()`.

---

### Wire protocol

Newline-delimited JSON (`\n`). First message from client **must** be `register`. Any language/runtime that can do stdio or TCP works.

#### Client → Daemon

| Command | Key fields |
|---------|-----------|
| `register` | `name` |
| `subscribe` | `tag` |
| `unsubscribe` | `tag` |
| `publish` | `to` (`"tag"` or `"tag:TargetName"`), `data` (object) |
| `launch` / `kill` / `restart` | `name` |
| `inject` | `target`, `name`, `url` or `source` |

#### Daemon → Client (push)

```json
{"push":true,"from":"tag","sender":"AppName","data":{...}}
```

Inject push (received as `onInjectionRequested`):
```json
{"push":true,"from":"__inject__","sender":"__bus__","data":{"inject":true,"name":"Panel","url":"..."}}
```

Process lifecycle events (sent to watchers):
```json
{"event":"launching","name":"X"}
{"event":"process_started","name":"X"}
{"event":"process_stopped","name":"X"}
{"event":"process_crashed","name":"X"}
{"event":"injected","name":"X"}
```

---

### QML patterns

**Proxy/foreign** — implement in plain QML using `{"properties":{...}}` / `{"action":"..."}` message shapes over `busClient.publish` / `onMessageReceived`. No C++ helper needed.

**QML injection** — `LaunchSpec { attachTo: "HostApp" }` in a panel's `Launch.qml`. The daemon sends an inject push to the host instead of spawning a new process. The host's `onInjectionRequested` creates the component via `Qt.createComponent(url)` or `Qt.createQmlObject(source, parent, name)`. Injected components run inside the host's QML engine and share its `busClient` context property.
