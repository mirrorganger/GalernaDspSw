# CubeMX-generated MCU bring-up: main.c, the IRQ table, HAL MSP config, and the newlib
# syscall stubs -- hand-maintained now, see docs/cubemx/README.md.

# Public include dir (main.h, stm32f4xx_hal_conf.h, stm32f4xx_it.h) -- also needed by
# drivers/ (every HAL source includes stm32f4xx_hal_conf.h to select which HAL modules
# to compile), so it's its own small INTERFACE library rather than folded into the
# per-app target_sources() below.
add_library(firmware_core INTERFACE)
target_include_directories(firmware_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# Attaches firmware_core's CubeMX-generated bring-up sources (main.c et al, plus the
# startup file) to an app-specific executable target. Called once per app (see
# galerna_add_app() in applications/CMakeLists.txt) so each app gets its own
# independently flashable .elf sharing this same CubeMX-generated bring-up code.
#
# Uses CMAKE_SOURCE_DIR (an absolute, scope-independent constant), not
# CMAKE_CURRENT_SOURCE_DIR -- a CMake function body resolves CMAKE_CURRENT_SOURCE_DIR
# using the scope of its *caller* (applications/CMakeLists.txt), not the directory
# where the function was defined, so that would silently resolve to the wrong path here.
function(galerna_configure_cubemx_target target)
    target_sources(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/firmware_core/src/main.c
        ${CMAKE_SOURCE_DIR}/firmware_core/src/stm32f4xx_it.c
        ${CMAKE_SOURCE_DIR}/firmware_core/src/stm32f4xx_hal_msp.c
        ${CMAKE_SOURCE_DIR}/firmware_core/src/sysmem.c
        ${CMAKE_SOURCE_DIR}/firmware_core/src/syscalls.c
        ${CMAKE_SOURCE_DIR}/startup_stm32f405xx.s
    )
    target_link_libraries(${target} PRIVATE
        firmware_core
        STM32_Drivers
        ${TOOLCHAIN_LINK_LIBRARIES}
    )
    set_target_properties(${target} PROPERTIES ADDITIONAL_CLEAN_FILES ${target}.map)
endfunction()

# Validate that the CubeMX-generated C code is compatible with the configured C standard.
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
