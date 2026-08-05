add_library(galerna_core INTERFACE)
add_library(galerna::core ALIAS galerna_core)

target_include_directories(galerna_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(galerna_core INTERFACE cxx_std_23)

# BinaryLedDisplay.hpp uses hal::Gpio.
target_link_libraries(galerna_core INTERFACE galerna::hal)

if(GALERNA_BUILD_TESTS)
    target_sources(galerna_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBinaryLedDisplay.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestDuplexAudioBlockProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestProcessorChain.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestStateVariableFilter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestTestTone.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestWavetableOscillator.cpp
    )
endif()
