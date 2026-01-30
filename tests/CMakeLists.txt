cmake_minimum_required(VERSION 3.16)

# Find Qt6Test
find_package(Qt6 REQUIRED COMPONENTS Core Test)

# Enable testing
enable_testing()

# UDM Test
add_executable(test_udm test_udm.cpp
    ${CMAKE_SOURCE_DIR}/src/core/UniversalDataModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataPoint.h
)

target_include_directories(test_udm PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(test_udm
    Qt6::Core
    Qt6::Test
)

add_test(NAME UDM_Test COMMAND test_udm)

# Channel Test
add_executable(test_channel test_channel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Channel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/UniversalDataModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataTypes.cpp
    ${CMAKE_SOURCE_DIR}/src/core/GlobalDataModel.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/interfaces.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/ModbusTcpCollector.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/ModbusTcpServer.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/ModbusRtuCollector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataPoint.h
    ${CMAKE_SOURCE_DIR}/src/adapters/interfaces.h
)

target_include_directories(test_channel PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/adapters
    ${MODBUS_INCLUDE_DIR}
)

target_link_libraries(test_channel
    Qt6::Core
    Qt6::Test
    ${MODBUS_LIBRARY}
)

if(WIN32)
    target_link_libraries(test_channel ws2_32)
endif()

add_test(NAME Channel_Test COMMAND test_channel)

# ModbusTcpCollector Test
add_executable(test_modbus_collector test_modbus_collector.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/interfaces.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/ModbusTcpCollector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/UniversalDataModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataTypes.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataPoint.h
    ${CMAKE_SOURCE_DIR}/src/adapters/interfaces.h
)

target_include_directories(test_modbus_collector PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/adapters
    ${MODBUS_INCLUDE_DIR}
)

target_link_libraries(test_modbus_collector
    Qt6::Core
    Qt6::Test
    ${MODBUS_LIBRARY}
)

if(WIN32)
    target_link_libraries(test_modbus_collector ws2_32)
endif()

add_test(NAME ModbusCollector_Test COMMAND test_modbus_collector)

# ModbusTcpServer Test
add_executable(test_modbus_server test_modbus_server.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/interfaces.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/ModbusTcpServer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/UniversalDataModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataTypes.cpp
    ${CMAKE_SOURCE_DIR}/src/core/GlobalDataModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataPoint.h
    ${CMAKE_SOURCE_DIR}/src/adapters/interfaces.h
)

target_include_directories(test_modbus_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/adapters
    ${MODBUS_INCLUDE_DIR}
)

target_link_libraries(test_modbus_server
    Qt6::Core
    Qt6::Test
    ${MODBUS_LIBRARY}
)

if(WIN32)
    target_link_libraries(test_modbus_server ws2_32)
endif()

add_test(NAME ModbusServer_Test COMMAND test_modbus_server)

# DataTypes Test
add_executable(test_datatypes test_datatypes.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataTypes.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataTypes.h
)

target_include_directories(test_datatypes PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(test_datatypes
    Qt6::Core
    Qt6::Test
)

add_test(NAME DataTypes_Test COMMAND test_datatypes)

# Integration Test
add_executable(test_integration test_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Channel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ChannelManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/UniversalDataModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DataTypes.cpp
    ${CMAKE_SOURCE_DIR}/src/core/GlobalDataModel.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/interfaces.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/ModbusTcpCollector.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/ModbusTcpServer.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/ModbusRtuCollector.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ConfigManager.cpp
)

target_include_directories(test_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/adapters
    ${CMAKE_SOURCE_DIR}/src/utils
    ${MODBUS_INCLUDE_DIR}
)

target_link_libraries(test_integration
    Qt6::Core
    Qt6::Test
    ${MODBUS_LIBRARY}
)

if(WIN32)
    target_link_libraries(test_integration ws2_32)
endif()

add_test(NAME Integration_Test COMMAND test_integration)
