add_library(galerna_hal INTERFACE)
add_library(galerna::hal ALIAS galerna_hal)

target_include_directories(galerna_hal INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(galerna_hal INTERFACE cxx_std_23)

# Test-only fakes for the hal concepts (FakeAdc/FakeGpio/FakeI2c), consumed by
# galerna::drivers's and galerna::app's tests -- never by production code.
add_library(galerna_hal_fakes INTERFACE)
add_library(galerna::hal::fakes ALIAS galerna_hal_fakes)

target_include_directories(galerna_hal_fakes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(galerna_hal_fakes INTERFACE galerna::hal)
