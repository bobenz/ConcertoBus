cmake_minimum_required(VERSION 3.16)
project(ConcertoBus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL     "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE        "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG          "RelWithDebInfo;")

# Qt6 required
find_package(Qt6 REQUIRED COMPONENTS Core Network Qml Quick)

option(CONCERTO_BUS_XMPP "Build XMPP gateway (requires QXmpp)" OFF)

if(CONCERTO_BUS_XMPP)
    list(APPEND CMAKE_PREFIX_PATH "D:/Qt/qxmpp-installed")
    find_package(QXmppQt6 REQUIRED)
endif()

# ── Client library ────────────────────────────────────────────────────────────
qt_add_library(ConcertoBusClient STATIC
    client/BusClient.h
    client/BusClient.cpp
)
target_include_directories(ConcertoBusClient PUBLIC client)
target_link_libraries(ConcertoBusClient PUBLIC Qt6::Core Qt6::Network)

# ── QML plugin ────────────────────────────────────────────────────────────────
qt_add_qml_module(ConcertoBusQml
    URI ConcertoBus
    VERSION 1.0
    PLUGIN_TARGET concertobusplugin
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/ConcertoBus
    SOURCES
        client/BusClientQml.h
        client/BusClientQml.cpp
        client/ConcertoBusPlugin.cpp
)
target_link_libraries(ConcertoBusQml PRIVATE ConcertoBusClient Qt6::Qml Qt6::Quick)

# ── Bus daemon ───────────────────────────────────────────────────────────────
set(BUS_SOURCES
    bus/main.cpp
    bus/BusServer.h
    bus/BusServer.cpp
    bus/Router.h
    bus/Router.cpp
    bus/Catalog.h
    bus/Catalog.cpp
)

if(CONCERTO_BUS_XMPP)
    list(APPEND BUS_SOURCES bus/XmppGateway.h bus/XmppGateway.cpp)
endif()

qt_add_executable(ConcertoBusDaemon ${BUS_SOURCES})
target_include_directories(ConcertoBusDaemon PRIVATE bus)
target_link_libraries(ConcertoBusDaemon PRIVATE Qt6::Core Qt6::Network)

if(CONCERTO_BUS_XMPP)
    target_link_libraries(ConcertoBusDaemon PRIVATE QXmpp::QXmpp)
    target_compile_definitions(ConcertoBusDaemon PRIVATE CONCERTO_BUS_XMPP)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Test QUIET)
if(Qt6Test_FOUND)
    enable_testing()

    qt_add_executable(tst_local
        tests/tst_local.cpp
        bus/BusServer.h bus/BusServer.cpp
        bus/Router.h    bus/Router.cpp
        bus/Catalog.h   bus/Catalog.cpp
    )
    target_include_directories(tst_local PRIVATE bus)
    target_link_libraries(tst_local PRIVATE ConcertoBusClient Qt6::Test Qt6::Network)
    add_test(NAME tst_local COMMAND tst_local)

    qt_add_executable(tst_reflection
        tests/tst_reflection.cpp
        bus/BusServer.h bus/BusServer.cpp
        bus/Router.h    bus/Router.cpp
        bus/Catalog.h   bus/Catalog.cpp
        client/BusClientQml.h client/BusClientQml.cpp
    )
    target_include_directories(tst_reflection PRIVATE bus client)
    target_link_libraries(tst_reflection PRIVATE ConcertoBusClient Qt6::Test Qt6::Network Qt6::Qml)
    add_test(NAME tst_reflection COMMAND tst_reflection)
endif()
