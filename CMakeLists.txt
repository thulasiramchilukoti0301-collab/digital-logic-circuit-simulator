cmake_minimum_required(VERSION 3.15)

project(DigitalLogicEngine LANGUAGES CXX)

include(CTest)

add_library(digital_logic_engine
    backend/src/Component.cpp
    backend/src/Gate.cpp
    backend/src/Input.cpp
    backend/src/Output.cpp
    backend/src/Wire.cpp
    backend/src/Circuit.cpp
    backend/src/ANDGate.cpp
    backend/src/ORGate.cpp
    backend/src/NOTGate.cpp
    backend/src/XORGate.cpp
    backend/src/NANDGate.cpp
    backend/src/NORGate.cpp
)

target_include_directories(digital_logic_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
)

target_compile_features(digital_logic_engine PUBLIC cxx_std_17)

set_target_properties(digital_logic_engine PROPERTIES
    CXX_EXTENSIONS OFF
)

# Vendored, header-only dependencies used by the future application layer.
# Keep them separate from digital_logic_engine so the simulation library
# remains independent of HTTP and JSON concerns.
add_library(cpp_httplib INTERFACE)
target_include_directories(cpp_httplib SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib
)
target_compile_features(cpp_httplib INTERFACE cxx_std_17)
if(WIN32)
    target_link_libraries(cpp_httplib INTERFACE ws2_32)
endif()

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json/include
)
target_compile_features(nlohmann_json INTERFACE cxx_std_17)

add_executable(digital_logic_server backend/app/main.cpp)
target_link_libraries(digital_logic_server
    PRIVATE
        digital_logic_engine
        cpp_httplib
)

if(BUILD_TESTING)
    add_executable(dependency_probe backend/tests/dependency_probe.cpp)
    target_link_libraries(dependency_probe PRIVATE cpp_httplib nlohmann_json)
    add_test(NAME dependency_probe COMMAND dependency_probe)

    add_executable(digital_logic_circuit_request_test
        backend/app/CircuitRequest.cpp
        backend/tests/circuit_request_test.cpp
    )
    target_include_directories(digital_logic_circuit_request_test PRIVATE backend/app)
    target_link_libraries(digital_logic_circuit_request_test
        PRIVATE digital_logic_engine nlohmann_json
    )
    add_test(NAME digital_logic_circuit_request_test
        COMMAND digital_logic_circuit_request_test
    )

    add_executable(digital_logic_smoke backend/tests/smoke.cpp)
    target_link_libraries(digital_logic_smoke PRIVATE digital_logic_engine)
    add_test(NAME digital_logic_smoke COMMAND digital_logic_smoke)

    add_executable(digital_logic_gates_test backend/tests/gates_test.cpp)
    target_link_libraries(digital_logic_gates_test PRIVATE digital_logic_engine)
    add_test(NAME digital_logic_gates_test COMMAND digital_logic_gates_test)

    add_executable(digital_logic_components_test backend/tests/components_test.cpp)
    target_link_libraries(digital_logic_components_test PRIVATE digital_logic_engine)
    add_test(NAME digital_logic_components_test COMMAND digital_logic_components_test)

    add_executable(digital_logic_circuit_validation_test
        backend/tests/circuit_validation_test.cpp
    )
    target_link_libraries(digital_logic_circuit_validation_test PRIVATE digital_logic_engine)
    add_test(NAME digital_logic_circuit_validation_test
        COMMAND digital_logic_circuit_validation_test
    )

    add_executable(digital_logic_circuit_evaluation_test
        backend/tests/circuit_evaluation_test.cpp
    )
    target_link_libraries(digital_logic_circuit_evaluation_test PRIVATE digital_logic_engine)
    add_test(NAME digital_logic_circuit_evaluation_test
        COMMAND digital_logic_circuit_evaluation_test
    )

    add_executable(digital_logic_truth_table_test
        backend/tests/truth_table_test.cpp
    )
    target_link_libraries(digital_logic_truth_table_test PRIVATE digital_logic_engine)
    add_test(NAME digital_logic_truth_table_test COMMAND digital_logic_truth_table_test)
endif()
