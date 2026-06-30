cmake_minimum_required(VERSION 3.16)
project(ConcertoBusDaemon VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL     "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE        "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG          "RelWithDebInfo;")

find_package(Qt6 REQUIRED COMPONENTS Core Network Qml)

option(CONCERTO_BUS_WS   "Build WebSocket transport plugin (requires Qt6::WebSockets)" OFF)
option(CONCERTO_BUS_XMPP "Build XMPP gateway plugin (requires QXmpp)" OFF)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ── BusConfig library ─────────────────────────────────────────────────────────
qt_add_library(BusConfigLib STATIC
    ${ROOT}/bus/config/BusConfigTypes.h
    ${ROOT}/bus/config/BusConfigLoader.h
    ${ROOT}/bus/config/BusConfigLoader.cpp
    ${ROOT}/bus/config/LaunchSpecReader.h
)
target_include_directories(BusConfigLib PUBLIC ${ROOT}/bus/config ${ROOT}/bus)
target_link_libraries(BusConfigLib PUBLIC Qt6::Core Qt6::Qml)

# ── ProcessManager library ────────────────────────────────────────────────────
qt_add_library(BusProcessManager STATIC
    ${ROOT}/bus/ProcessManager.h
    ${ROOT}/bus/ProcessManager.cpp
)
target_include_directories(BusProcessManager PUBLIC ${ROOT}/bus)
target_link_libraries(BusProcessManager PUBLIC Qt6::Core BusConfigLib)

# ── StdioTransport (built-in, statically linked) ──────────────────────────────
qt_add_library(BusStdioTransport STATIC
    ${ROOT}/bus/IBusTransport.h
    ${ROOT}/bus/IBusTransport.cpp
    ${ROOT}/transports/stdio/StdioTransport.h
    ${ROOT}/transports/stdio/StdioTransport.cpp
)
target_include_directories(BusStdioTransport PUBLIC ${ROOT}/transports/stdio ${ROOT}/bus)
target_link_libraries(BusStdioTransport PUBLIC Qt6::Core)

# ── Router ────────────────────────────────────────────────────────────────────
qt_add_library(BusRouter STATIC
    ${ROOT}/bus/Router.h
    ${ROOT}/bus/Router.cpp
)
target_include_directories(BusRouter PUBLIC ${ROOT}/bus)
target_link_libraries(BusRouter PUBLIC Qt6::Core)

# ── BusCore ───────────────────────────────────────────────────────────────────
qt_add_library(BusCoreLib STATIC
    ${ROOT}/bus/IBusGateway.h
    ${ROOT}/bus/IBusGateway.cpp
    ${ROOT}/bus/BusCore.h
    ${ROOT}/bus/BusCore.cpp
)
target_include_directories(BusCoreLib PUBLIC ${ROOT}/bus ${ROOT}/transports/stdio)
target_link_libraries(BusCoreLib PUBLIC Qt6::Core BusRouter BusStdioTransport BusProcessManager)

# ── TCP transport plugin ──────────────────────────────────────────────────────
set(PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/plugins)
qt_add_plugin(TcpTransportPlugin MODULE
    CLASS_NAME TcpTransport
    ${ROOT}/bus/IBusTransport.h
    ${ROOT}/bus/IBusTransport.cpp
    ${ROOT}/transports/tcp/TcpTransport.h
    ${ROOT}/transports/tcp/TcpTransport.cpp
)
set_target_properties(TcpTransportPlugin PROPERTIES
    OUTPUT_NAME "TcpTransport"
    LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
)
target_include_directories(TcpTransportPlugin PRIVATE ${ROOT}/transports/tcp ${ROOT}/bus)
target_link_libraries(TcpTransportPlugin PRIVATE Qt6::Core Qt6::Network)

# ── XMPP gateway plugin (optional) ───────────────────────────────────────────
if(CONCERTO_BUS_XMPP)
    list(APPEND CMAKE_PREFIX_PATH "D:/Qt/qxmpp-installed")
    find_package(QXmppQt6 REQUIRED)
    qt_add_plugin(XmppGatewayPlugin MODULE
        CLASS_NAME XmppGateway
        ${ROOT}/bus/IBusGateway.h
        ${ROOT}/gateways/xmpp/XmppGateway.h
        ${ROOT}/gateways/xmpp/XmppGateway.cpp
    )
    set_target_properties(XmppGatewayPlugin PROPERTIES
        OUTPUT_NAME "XmppGateway"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/gateways"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/gateways"
    )
    target_include_directories(XmppGatewayPlugin PRIVATE ${ROOT}/bus)
    target_link_libraries(XmppGatewayPlugin PRIVATE Qt6::Core Qt6::Network QXmpp::QXmpp)
endif()

# ── Daemon executable (pm.exe) ────────────────────────────────────────────────
qt_add_executable(ConcertoBusDaemon ${ROOT}/bus/main.cpp)
set_target_properties(ConcertoBusDaemon PROPERTIES OUTPUT_NAME "pm")
target_include_directories(ConcertoBusDaemon PRIVATE ${ROOT}/bus)
target_link_libraries(ConcertoBusDaemon PRIVATE
    Qt6::Core Qt6::Network Qt6::Qml
    BusConfigLib BusProcessManager BusCoreLib
)

# ── Tests ─────────────────────────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Test QUIET)
if(Qt6Test_FOUND)
    enable_testing()

    get_target_property(_qt_bin_dir Qt6::Core LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_bin_dir}" DIRECTORY)
    set(TEST_ENV "PATH=${_qt_bin_dir};$ENV{PATH}")

    qt_add_executable(tst_config ${ROOT}/tests/tst_config.cpp)
    target_include_directories(tst_config PRIVATE ${ROOT}/bus)
    target_link_libraries(tst_config PRIVATE BusConfigLib Qt6::Test)
    add_test(NAME tst_config COMMAND tst_config -o ${CMAKE_BINARY_DIR}/tst_config_result.txt,txt)
    set_tests_properties(tst_config PROPERTIES ENVIRONMENT "${TEST_ENV}")

    qt_add_executable(tst_processmanager ${ROOT}/tests/tst_processmanager.cpp)
    target_include_directories(tst_processmanager PRIVATE ${ROOT}/bus)
    target_link_libraries(tst_processmanager PRIVATE BusProcessManager Qt6::Test)
    add_test(NAME tst_processmanager COMMAND tst_processmanager -o ${CMAKE_BINARY_DIR}/tst_processmanager_result.txt,txt)
    set_tests_properties(tst_processmanager PROPERTIES ENVIRONMENT "${TEST_ENV}")

    qt_add_executable(tst_router ${ROOT}/tests/tst_router.cpp)
    target_include_directories(tst_router PRIVATE ${ROOT}/bus)
    target_link_libraries(tst_router PRIVATE BusRouter Qt6::Test)
    add_test(NAME tst_router COMMAND tst_router -o ${CMAKE_BINARY_DIR}/tst_router_result.txt,txt)
    set_tests_properties(tst_router PROPERTIES ENVIRONMENT "${TEST_ENV}")

    # stdio_echo_helper — raw protocol, no client lib dependency
    qt_add_executable(stdio_echo_helper ${ROOT}/tests/stdio_echo_helper/main.cpp)
    target_link_libraries(stdio_echo_helper PRIVATE Qt6::Core)

    qt_add_executable(tst_stdio ${ROOT}/tests/tst_stdio.cpp)
    target_include_directories(tst_stdio PRIVATE ${ROOT}/bus ${ROOT}/transports/stdio)
    target_link_libraries(tst_stdio PRIVATE BusCoreLib Qt6::Test)
    add_test(NAME tst_stdio COMMAND tst_stdio -o ${CMAKE_BINARY_DIR}/tst_stdio_result.txt,txt)
    set_tests_properties(tst_stdio PROPERTIES ENVIRONMENT "${TEST_ENV}")
    add_dependencies(tst_stdio stdio_echo_helper)

    qt_add_executable(tst_buscore ${ROOT}/tests/tst_buscore.cpp)
    target_include_directories(tst_buscore PRIVATE ${ROOT}/bus ${ROOT}/transports/stdio)
    target_link_libraries(tst_buscore PRIVATE BusCoreLib BusProcessManager Qt6::Test)
    add_test(NAME tst_buscore COMMAND tst_buscore -o ${CMAKE_BINARY_DIR}/tst_buscore_result.txt,txt)
    set_tests_properties(tst_buscore PROPERTIES ENVIRONMENT "${TEST_ENV}")
    add_dependencies(tst_buscore stdio_echo_helper TcpTransportPlugin)

    qt_add_executable(tst_qt5client ${ROOT}/tests/tst_qt5client.cpp ${ROOT}/demo/Qt5ClientApp/RawBusClient.h)
    target_include_directories(tst_qt5client PRIVATE ${ROOT}/bus ${ROOT}/demo/Qt5ClientApp)
    target_link_libraries(tst_qt5client PRIVATE BusCoreLib Qt6::Test)
    add_test(NAME tst_qt5client COMMAND tst_qt5client -o ${CMAKE_BINARY_DIR}/tst_qt5client_result.txt,txt)
    set_tests_properties(tst_qt5client PROPERTIES ENVIRONMENT "${TEST_ENV}")
    add_dependencies(tst_qt5client TcpTransportPlugin)
endif()
