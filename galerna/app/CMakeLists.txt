add_library(galerna_app INTERFACE)
add_library(galerna::app ALIAS galerna_app)

target_include_directories(galerna_app INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(galerna_app INTERFACE cxx_std_23)

# GalernaApp.hpp takes the pot mux (galerna::drivers) and is constrained on hal::Adc/hal::Gpio.
target_link_libraries(galerna_app INTERFACE
    galerna::drivers
    galerna::hal
)

if(GALERNA_BUILD_TESTS)
    target_sources(galerna_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestGalernaApp.cpp
    )
endif()
