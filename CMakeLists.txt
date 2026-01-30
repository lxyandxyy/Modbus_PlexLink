cmake_minimum_required(VERSION 3.16)
project(Modbus_PlexLink VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find required Qt6 components (including WebSockets for remote API, Sql for alarm database)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Concurrent WebSockets PrintSupport Sql)

message(STATUS "Qt6 Concurrent found: ${Qt6Concurrent_FOUND}")
message(STATUS "Qt6 WebSockets found: ${Qt6WebSockets_FOUND}")
message(STATUS "Qt6 Sql found: ${Qt6Sql_FOUND}")

# Set third party directory
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty)

# Configure libmodbus
if(WIN32)
    # Windows: Use ThirdParty directory
    set(MODBUS_INCLUDE_DIR ${THIRD_PARTY_DIR}/libmodbus)
    set(MODBUS_LIBRARY ${THIRD_PARTY_DIR}/libmodbus/modbus.lib)
    set(MODBUS_DLL ${THIRD_PARTY_DIR}/libmodbus/modbus.dll)
    
    if(NOT EXISTS ${MODBUS_INCLUDE_DIR}/modbus.h)
        message(FATAL_ERROR "libmodbus header not found at: ${MODBUS_INCLUDE_DIR}")
    endif()
    
    if(NOT EXISTS ${MODBUS_LIBRARY})
        message(FATAL_ERROR "libmodbus library not found at: ${MODBUS_LIBRARY}")
    endif()
    
    message(STATUS "Using libmodbus from: ${THIRD_PARTY_DIR}/libmodbus")
else()
    # Linux/Unix: Try to find system libmodbus
    find_path(MODBUS_INCLUDE_DIR modbus.h
        HINTS /usr/include/modbus /usr/local/include/modbus
        PATH_SUFFIXES modbus
    )
    
    find_library(MODBUS_LIBRARY
        NAMES modbus
        HINTS /usr/lib /usr/local/lib
    )
    
    if(NOT MODBUS_INCLUDE_DIR OR NOT MODBUS_LIBRARY)
        message(FATAL_ERROR "libmodbus not found! Please install libmodbus-dev")
    endif()
endif()

# Configure libcurl (optional)
set(CURL_FOUND FALSE)
if(WIN32)
    set(CURL_INCLUDE_DIR ${THIRD_PARTY_DIR}/libcurl/include)
    set(CURL_LIBRARY ${THIRD_PARTY_DIR}/libcurl/libcurl_64bit.lib)
    
    if(EXISTS ${CURL_INCLUDE_DIR}/curl/curl.h AND EXISTS ${CURL_LIBRARY})
        set(CURL_FOUND TRUE)
        message(STATUS "Using libcurl from: ${THIRD_PARTY_DIR}/libcurl")
    else()
        message(STATUS "libcurl not found in ThirdParty directory (optional)")
    endif()
else()
    # Linux/Unix: Try to find system libcurl
    find_package(CURL QUIET)
    if(CURL_FOUND)
        message(STATUS "Using system libcurl")
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/adapters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/remote
    ${MODBUS_INCLUDE_DIR}
)

# Add libcurl include directory if found
if(CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIR})
    add_definitions(-DHAS_CURL_SUPPORT)
endif()

# ============================================================================
# Collect source files
# ============================================================================

# Core sources (shared between GUI and Service)
file(GLOB CORE_SOURCES
    src/core/*.cpp
    src/core/*.h
)

# Adapter sources (shared)
file(GLOB ADAPTER_SOURCES
    src/adapters/*.cpp
    src/adapters/*.h
)

# Utils sources (shared)
file(GLOB UTILS_SOURCES
    src/utils/*.cpp
    src/utils/*.h
)

# Remote API sources (shared)
file(GLOB REMOTE_SOURCES
    src/remote/*.cpp
    src/remote/*.h
)

# GUI sources (only for GUI app)
file(GLOB GUI_SOURCES
    src/gui/*.cpp
    src/gui/*.h
)

file(GLOB_RECURSE UI_FILES
    src/gui/*.ui
)

# Common sources (core + adapters + utils + remote)
set(COMMON_SOURCES
    ${CORE_SOURCES}
    ${ADAPTER_SOURCES}
    ${UTILS_SOURCES}
    ${REMOTE_SOURCES}
)

# ============================================================================
# Target 1: GUI Application (Modbus_PlexLink)
# ============================================================================
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${COMMON_SOURCES}
    ${GUI_SOURCES}
    ${UI_FILES}
)

# Fix "too many sections" error for large source files (QCustomPlot) on MinGW
if(MINGW)
    # Apply -Wa,-mbig-obj to allow large object files
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/qcustomplot.cpp
        PROPERTIES COMPILE_FLAGS "-Wa,-mbig-obj"
    )
    message(STATUS "Applied -Wa,-mbig-obj for QCustomPlot (MinGW)")
endif()

# Link PrintSupport for QCustomPlot PDF export
find_package(Qt6 COMPONENTS PrintSupport QUIET)
if(Qt6PrintSupport_FOUND)
    message(STATUS "Qt6 PrintSupport found - PDF export enabled")
endif()

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::WebSockets
    Qt6::PrintSupport
    Qt6::Sql
    ${MODBUS_LIBRARY}
)

if(CURL_FOUND)
    target_link_libraries(${PROJECT_NAME} ${CURL_LIBRARY})
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

# ============================================================================
# Target 2: Headless Service (Modbus_PlexLink_Service)
# ============================================================================
add_executable(${PROJECT_NAME}_Service
    src/service_main.cpp
    ${COMMON_SOURCES}
)

target_link_libraries(${PROJECT_NAME}_Service
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    Qt6::WebSockets
    Qt6::Sql
    ${MODBUS_LIBRARY}
)

if(CURL_FOUND)
    target_link_libraries(${PROJECT_NAME}_Service ${CURL_LIBRARY})
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME}_Service ws2_32)
endif()

# Service runs in console mode (not a GUI app)
set_target_properties(${PROJECT_NAME}_Service PROPERTIES
    WIN32_EXECUTABLE FALSE
)

# ============================================================================
# Windows specific settings
# ============================================================================
if(WIN32)
    # GUI app runs without console
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    # Note: Qt DLL deployment is handled by windeployqt in CI/CD workflows
    # The following code is kept as a fallback for local builds
    # For production builds, use windeployqt for complete deployment
    
    # Try to find Qt DLLs and copy them (fallback method)
    # This may not work correctly in all cases, windeployqt is preferred
    if(EXISTS "${Qt6_DIR}/bin")
        # GUI app DLLs
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${Qt6_DIR}/bin/Qt6Core.dll"
            "${Qt6_DIR}/bin/Qt6Gui.dll"
            "${Qt6_DIR}/bin/Qt6Widgets.dll"
            "${Qt6_DIR}/bin/Qt6Network.dll"
            "${Qt6_DIR}/bin/Qt6Concurrent.dll"
            "${Qt6_DIR}/bin/Qt6WebSockets.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying Qt DLLs (fallback - use windeployqt for production)"
        )
        
        # Service DLLs
        add_custom_command(TARGET ${PROJECT_NAME}_Service POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${Qt6_DIR}/bin/Qt6Core.dll"
            "${Qt6_DIR}/bin/Qt6Network.dll"
            "${Qt6_DIR}/bin/Qt6Concurrent.dll"
            "${Qt6_DIR}/bin/Qt6WebSockets.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}_Service>
            COMMENT "Copying Qt DLLs for Service (fallback - use windeployqt for production)"
        )
    endif()
    
    # Copy modbus.dll to output directory
    if(EXISTS ${MODBUS_DLL})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MODBUS_DLL}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
        add_custom_command(TARGET ${PROJECT_NAME}_Service POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MODBUS_DLL}
            $<TARGET_FILE_DIR:${PROJECT_NAME}_Service>
        )
        message(STATUS "Will copy modbus.dll from: ${MODBUS_DLL}")
    endif()
endif()

# ============================================================================
# Installation rules
# ============================================================================
install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_Service
    RUNTIME DESTINATION bin
)

# Print build summary
message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Build targets:")
message(STATUS "    - ${PROJECT_NAME} (GUI Application)")
message(STATUS "    - ${PROJECT_NAME}_Service (Headless Service)")
message(STATUS "")

# Enable testing (optional, can be disabled with -DBUILD_TESTING=OFF)
option(BUILD_TESTING "Build the testing tree" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
