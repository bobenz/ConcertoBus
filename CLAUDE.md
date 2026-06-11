# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires Qt 6 (Core, Network, Qml, Quick) and MSVC 2022 on Windows. Qt is expected at `D:\Qt\6.10.3\msvc2022_64`.

```powershell
# Standard build (no XMPP)
cmake -B build -G "Ninja" -DCMAKE_PREFIX_PATH="D:/Qt/6.10.3/msvc2022_64"
cmake --build build --config RelWithDebInfo

# With XMPP federation (requires QXmpp installed at D:\Qt\qxmpp-installed)
cmake -B build/xmpp-test -DCONCERTO_BUS_XMPP=ON -DCMAKE_PREFIX_PATH="D:/Qt/6.10.3/msvc2022_64"
cmake --build build/xmpp-test --config RelWithDebInfo
```

## Tests

```powershell
cd build
ctest -C RelWithDebInfo --output-on-failure

# Run a single test binary directly
.\RelWithDebInfo\tst_local.exe
.\RelWithDebInfo\tst_reflection.exe
```

## Deploy (Windows)

```powershell
.\deploy\Deploy-ConcertoBus.ps1                    # XMPP build, defaults
.\deploy\Deploy-ConcertoBus.ps1 -WithXmpp:$false   # no-XMPP build
```

Output lands in `dist\ConcertoBus-1.0-Win64\`. See script header for all parameters.

## Architecture

ConcertoBus is a named-pipe-style local message bus over TCP. It has three components:

### Daemon (`bus/`)

- **`BusServer`** — TCP server (default port 49152). Accepts JSON-line connections, drives `Router` and `Catalog`, manages `m_launchWaiters` for async process-launch flow.
- **`Router`** — pure routing logic: registry (name→socket), subscriptions (topic→sockets), offline queues. Emits `published` signal used by `XmppGateway`.
- **`Catalog`** — loads a `catalog.json` file mapping logical names to executables. Called by `BusServer` for `launch` commands.
- **`XmppGateway`** (optional, `CONCERTO_BUS_XMPP`) — federates with a remote bus over XMPP. Local nodes appear as XMPP presence resources; remote subscriptions are forwarded via IQ stanzas and delivered as `<message>` stanzas.

Wire protocol: newline-delimited JSON over TCP. Command fields: `cmd` (`register`, `subscribe`, `unsubscribe`, `publish`, `launch`), `name`, `topic`, `data`.

### Client library (`client/`)

- **`BusClient`** (C++) — `QObject` wrapping a `QTcpSocket`. Methods: `connectToBus()`, `subscribe()`, `unsubscribe()`, `publish()`, `launch()`. Emits `messageReceived(from, data)`.
- **`BusClientQml`** — two QML elements exported under `import ConcertoBus 1.0`:
  - **`BusForeign`** — subscribes to a topic and applies incoming JSON to its own QML properties/signals via `QMetaObject` reflection (property reflection from `propertyOffset` to skip Qt built-ins).
  - **`BusProxy`** — hooks a parent QML object's user-defined properties and non-`Changed` signals (via `SignalRelay` objects) and publishes changes as `{"properties":{...}}` or `{"action":...}` messages. Sends a full property snapshot on every reconnect.

### QML plugin

Built as `ConcertoBusQml` (library) + `concertobusplugin` (plugin DLL), output to `imports/ConcertoBus/`. Client apps set `QML_IMPORT_PATH` to the `imports/` parent.

### Reflection pattern

Both `BusForeign` and `BusProxy` use `QMetaObject` property/method iteration starting at `metaObject()->propertyOffset()` / `methodOffset()` to operate only on user-defined members, skipping Qt-internal ones. `BusProxy` uses `SignalRelay` helper objects to intercept property changes without modifying the target type.
