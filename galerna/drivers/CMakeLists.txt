add_library(galerna_drivers INTERFACE)
add_library(galerna::drivers ALIAS galerna_drivers)

target_include_directories(galerna_drivers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(galerna_drivers INTERFACE cxx_std_23)

# PotMux4051.hpp/Es8388Codec.hpp are constrained on hal::Adc/hal::Gpio/hal::I2cBus.
target_link_libraries(galerna_drivers INTERFACE galerna::hal)

if(GALERNA_BUILD_TESTS)
    target_sources(galerna_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestEs8388Codec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPotMux4051.cpp
    )
endif()
