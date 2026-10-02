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
)

target_include_directories(digital_logic_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
)

target_compile_features(digital_logic_engine PUBLIC cxx_std_17)

set_target_properties(digital_logic_engine PROPERTIES
    CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
    add_executable(digital_logic_smoke backend/tests/smoke.cpp)
    target_link_libraries(digital_logic_smoke PRIVATE digital_logic_engine)
    add_test(NAME digital_logic_smoke COMMAND digital_logic_smoke)

    add_executable(digital_logic_gates_test backend/tests/gates_test.cpp)
    target_link_libraries(digital_logic_gates_test PRIVATE digital_logic_engine)
    add_test(NAME digital_logic_gates_test COMMAND digital_logic_gates_test)
endif()
