# XMPP Federation Demo

Two runners on separate machines (or in two terminals on one machine).
SensorApp lives on Runner A; DisplayApp lives on Runner B.
XMPP bridges the two buses so Display receives Sensor's messages transparently.

## Topology

```
Runner A (client1@xmpp.credics)        Runner B (client2@xmpp.credics)
  └─ SensorApp (stdio)          XMPP      └─ DisplayApp (stdio)
      publishes "sensor"  ─────────────►  subscribes "sensor"
```

## Build

```powershell
cmake -B build -DCONCERTO_BUS_XMPP=ON -DCMAKE_PREFIX_PATH="D:/Qt/6.10.3/msvc2022_64"
cmake --build build --config RelWithDebInfo
```

## Run (from project root)

**Runner A** (start first):
```
build\RelWithDebInfo\pm.exe -c demo\xmpp-demo\runner-a\config.qml
```

**Runner B** (any order):
```
build\RelWithDebInfo\pm.exe -c demo\xmpp-demo\runner-b\config.qml
```

Once both runners connect to `xmpp.credics`, DisplayApp will receive
the sensor messages published by SensorApp.

## What happens

1. Both runners connect to the XMPP server as `client1@...` and `client2@...`
2. Runner A launches SensorApp; it registers on the local bus as "Sensor"
   → Runner A sends XMPP presence `client1@xmpp.credics/Sensor` (Available)
   → Runner B sees Sensor is alive on A
3. Runner B launches DisplayApp; it subscribes to tag `sensor`
   → Runner B sends `bus_subscribe` stanza to Runner A
   → Runner A adds Runner B to its relay list for "sensor"
4. SensorApp publishes to `sensor`
   → Runner A sends `bus_relay` stanza to Runner B
   → Runner B injects the message into its local bus
   → DisplayApp receives it
