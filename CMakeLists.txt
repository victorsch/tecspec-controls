cmake_minimum_required(VERSION 3.16)
project(heatex VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Svg)

# BACnet stack paths - use local copy in repo
set(BACNET_STACK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bacnet-stack")
set(BACNET_LIB_DIR "${BACNET_STACK_DIR}/build")

# Application sources
set(APP_SOURCES
    src/main.cpp
    src/config.cpp
    src/sensor_manager.cpp
    src/alarm_manager.cpp
    src/bacnet_interface.cpp
    src/mainwindow.cpp
)

# Create executable
add_executable(heatex ${APP_SOURCES})

# Include directories
target_include_directories(heatex PRIVATE
    ${BACNET_STACK_DIR}/src
    ${BACNET_STACK_DIR}/ports/linux
    src
)

# Compile definitions for BACnet stack
target_compile_definitions(heatex PRIVATE
    BACDL_BIP=1
    BBMD_ENABLED=1
    BBMD_CLIENT_ENABLED=1
    MAX_TSM_TRANSACTIONS=255
    MAX_ADDRESS_CACHE=255
    MAX_APDU=1476
)

# Link libraries
target_link_libraries(heatex PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::Svg
    ${BACNET_LIB_DIR}/libbacnet-stack.a
    pthread
    qrencode
)

# Set output directory
set_target_properties(heatex PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
