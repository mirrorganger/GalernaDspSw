# Not host-tested: this is a thin HAL wrapper with no logic of its own -- see the
# Platform testability rule in CLAUDE.md. Stm32I2sDuplexAudioCallbacks.cpp is deliberately
# NOT a source here: it must be compiled directly into each app executable, not into this
# static archive, so the linker is always forced to pull in its strong HAL callback
# overrides (see applications/CMakeLists.txt for the full explanation).
add_library(galerna_platform STATIC
    src/stm32f405/Stm32Gpio.cpp
    src/stm32f405/Stm32I2c.cpp
    src/stm32f405/Stm32Adc.cpp
)
add_library(galerna::platform ALIAS galerna_platform)

target_include_directories(galerna_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(galerna_platform PUBLIC
    galerna::core
    stm32cubemx
)

target_compile_definitions(galerna_platform PUBLIC
    GALERNA_STM32_TARGET
)

# Remove wrong libob.a library dependency when using C++ files.
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
