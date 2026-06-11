cmake_minimum_required(VERSION 3.16)
project(ConcertoBus VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL     "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE        "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG          "RelWithDebInfo;")

find_package(Qt6 REQUIRED COMPONENTS Core Network Qml Quick)

option(CONCERTO_BUS_WS   "Build WebSocket transport plugin (requires Qt6::WebSockets)" OFF)
option(CONCERTO_BUS_XMPP "Build XMPP gateway plugin (requires QXmpp)" OFF)

# ── Shared plugin output directory ────────────────────────────────────────────
set(PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/plugins)

# ── BusConfig library (QML config types + loader) ─────────────────────────────
qt_add_library(BusConfigLib STATIC
    bus/config/BusConfigTypes.h
    bus/config/BusConfigLoader.h
    bus/config/BusConfigLoader.cpp
)
target_include_directories(BusConfigLib PUBLIC bus/config bus)
target_link_libraries(BusConfigLib PUBLIC Qt6::Core Qt6::Qml)

# ── ProcessManager library ────────────────────────────────────────────────────
qt_add_library(BusProcessManager STATIC
    bus/ProcessManager.h
    bus/ProcessManager.cpp
)
target_include_directories(BusProcessManager PUBLIC bus)
target_link_libraries(BusProcessManager PUBLIC Qt6::Core BusConfigLib)

# ── StdioTransport (built-in, statically linked into daemon) ──────────────────
# Written in Step 2 — placeholder target defined here so daemon can link it.
# qt_add_library(BusStdioTransport STATIC
#     transports/stdio/StdioTransport.h
#     transports/stdio/StdioTransport.cpp
# )
# target_include_directories(BusStdioTransport PUBLIC transports/stdio bus)
# target_link_libraries(BusStdioTransport PUBLIC Qt6::Core)

# ── TCP transport plugin ──────────────────────────────────────────────────────
# Written in Step 3 — placeholder.
# qt_add_plugin(TcpTransportPlugin MODULE
#     transports/tcp/TcpTransport.h
#     transports/tcp/TcpTransport.cpp
# )
# set_target_properties(TcpTransportPlugin PROPERTIES
#     LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR})
# target_include_directories(TcpTransportPlugin PRIVATE transports/tcp bus)
# target_link_libraries(TcpTransportPlugin PRIVATE Qt6::Core Qt6::Network)

# ── WebSocket transport plugin (optional) ─────────────────────────────────────
if(CONCERTO_BUS_WS)
    find_package(Qt6 REQUIRED COMPONENTS WebSockets)
    # Written in Step 4 — placeholder.
    # qt_add_plugin(WsTransportPlugin MODULE ...)
endif()

# ── XMPP gateway plugin (optional) ───────────────────────────────────────────
if(CONCERTO_BUS_XMPP)
    list(APPEND CMAKE_PREFIX_PATH "D:/Qt/qxmpp-installed")
    find_package(QXmppQt6 REQUIRED)
    # Written in Step 6 — placeholder.
    # qt_add_plugin(XmppGatewayPlugin MODULE ...)
endif()

# ── Client library ────────────────────────────────────────────────────────────
qt_add_library(ConcertoBusClient STATIC
    client/BusClient.h
    client/BusClient.cpp
)
target_include_directories(ConcertoBusClient PUBLIC client)
target_link_libraries(ConcertoBusClient PUBLIC Qt6::Core Qt6::Network)

# ── QML plugin (exports BusClient; StdioBusClient added in Step 2) ────────────
qt_add_qml_module(ConcertoBusQml
    URI ConcertoBus
    VERSION 2.0
    PLUGIN_TARGET concertobusplugin
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/ConcertoBus
    SOURCES
        client/ConcertoBusPlugin.cpp
)
target_link_libraries(ConcertoBusQml PRIVATE ConcertoBusClient Qt6::Qml Qt6::Quick)

# ── Bus daemon ────────────────────────────────────────────────────────────────
qt_add_executable(ConcertoBusDaemon
    bus/main.cpp
    bus/IBusTransport.h
    bus/IBusGateway.h
)
target_include_directories(ConcertoBusDaemon PRIVATE bus)
target_link_libraries(ConcertoBusDaemon PRIVATE
    Qt6::Core Qt6::Network Qt6::Qml
    BusConfigLib
    BusProcessManager
)

# ── Tests ─────────────────────────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Test QUIET)
if(Qt6Test_FOUND)
    enable_testing()

    # Put Qt DLLs on PATH for all tests so they can be run directly by ctest
    get_target_property(_qt_bin_dir Qt6::Core LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_bin_dir}" DIRECTORY)
    set(TEST_ENV "PATH=${_qt_bin_dir};$ENV{PATH}")

    # Step 1 — config parsing
    qt_add_executable(tst_config tests/tst_config.cpp)
    target_include_directories(tst_config PRIVATE bus)
    target_link_libraries(tst_config PRIVATE BusConfigLib Qt6::Test)
    add_test(NAME tst_config COMMAND tst_config -o ${CMAKE_BINARY_DIR}/tst_config_result.txt,txt)
    set_tests_properties(tst_config PROPERTIES ENVIRONMENT "${TEST_ENV}")

    # Step 1 — process manager
    qt_add_executable(tst_processmanager tests/tst_processmanager.cpp)
    target_include_directories(tst_processmanager PRIVATE bus)
    target_link_libraries(tst_processmanager PRIVATE BusProcessManager Qt6::Test)
    add_test(NAME tst_processmanager COMMAND tst_processmanager -o ${CMAKE_BINARY_DIR}/tst_processmanager_result.txt,txt)
    set_tests_properties(tst_processmanager PROPERTIES ENVIRONMENT "${TEST_ENV}")

    # Legacy tests (kept until superseded in later steps)
    qt_add_executable(tst_local
        tests/tst_local.cpp
        bus/BusServer.h bus/BusServer.cpp
        bus/Router.h    bus/Router.cpp
        bus/Catalog.h   bus/Catalog.cpp
    )
    target_include_directories(tst_local PRIVATE bus)
    target_link_libraries(tst_local PRIVATE ConcertoBusClient Qt6::Test Qt6::Network)
    add_test(NAME tst_local COMMAND tst_local)
    set_tests_properties(tst_local PROPERTIES ENVIRONMENT "${TEST_ENV}")
endif()
