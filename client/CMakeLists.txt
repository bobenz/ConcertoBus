cmake_minimum_required(VERSION 3.16)
project(ConcertoBusClient VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL     "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE        "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG          "RelWithDebInfo;")

find_package(Qt6 REQUIRED COMPONENTS Core Network Qml Quick)

# ── Client library ────────────────────────────────────────────────────────────
qt_add_library(ConcertoBusClient STATIC
    AbstractBusClient.h
    AbstractBusClient.cpp
    BusClient.h
    BusClient.cpp
    LaunchSpec.h
    LaunchSpec.cpp
    StdioBusClient.h
    StdioBusClient.cpp
)
target_include_directories(ConcertoBusClient PUBLIC .)
target_link_libraries(ConcertoBusClient PUBLIC Qt6::Core Qt6::Network Qt6::Qml)

# ── QML plugin (ConcertoBus 2.0) ──────────────────────────────────────────────
qt_add_qml_module(ConcertoBusQml
    URI ConcertoBus
    VERSION 2.0
    PLUGIN_TARGET concertobusplugin
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/ConcertoBus
    SOURCES
        AbstractBusClient.h  AbstractBusClient.cpp
        BusClient.h          BusClient.cpp
        LaunchSpec.h         LaunchSpec.cpp
        StdioBusClient.h     StdioBusClient.cpp
        ConcertoBusPlugin.cpp
)
target_link_libraries(ConcertoBusQml PRIVATE ConcertoBusClient Qt6::Qml Qt6::Quick)

# ── Generic QML host (client.exe) ─────────────────────────────────────────────
qt_add_executable(ConcertoBusHost host/main.cpp)
set_target_properties(ConcertoBusHost PROPERTIES OUTPUT_NAME "client")
target_include_directories(ConcertoBusHost PRIVATE .)
target_link_libraries(ConcertoBusHost PRIVATE
    ConcertoBusClient Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
)
