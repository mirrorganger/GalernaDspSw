# Defines one independently-buildable, independently-flashable firmware executable for
# the app living in applications/<name>/app.cpp. Every app shares the same CubeMX bring-up
# (main.c et al, via galerna_configure_cubemx_target(), defined in
# firmware_core/CMakeLists.txt), HAL drivers, and galerna platform/core/effects/
# drivers/app libraries -- only applications/<name>/app.cpp (the hardware wiring + which
# galerna::app/galerna::effects classes to run) differs between them.
function(galerna_add_app name)
    add_executable(${name})
    target_sources(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/${name}/app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/common/SwoDebug.cpp
        # Directly on the executable target, not in galerna::platform (a static archive):
        # HAL's own stm32f4xx_hal_i2s_ex.c.o both calls and __weak-defines these same
        # callback names, so it self-satisfies the reference and the linker never has an
        # unresolved symbol to pull this override in for if it's just an unreferenced
        # archive member. Being a directly-compiled source of the executable makes it
        # unconditionally linked.
        ${CMAKE_SOURCE_DIR}/galerna/platform/src/stm32f405/Stm32I2sDuplexAudioCallbacks.cpp
    )

    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_SOURCE_DIR}
    )

    # Every app links the full set of galerna libraries -- which ones an individual
    # app.cpp actually uses varies (e.g. thx_deep_note needs galerna::effects,
    # pot_blink needs galerna::app), so link them all rather than tuning per app.
    target_link_libraries(${name} PRIVATE
        galerna::core
        galerna::effects
        galerna::drivers
        galerna::app
        galerna::platform
        stm32cubemx
    )

    galerna_configure_cubemx_target(${name})
    target_link_libraries(${name} PRIVATE STM32_Drivers)
endfunction()

galerna_add_app(thx_deep_note)
galerna_add_app(pot_blink)
galerna_add_app(wind_chimes)
galerna_add_app(twin_pluck)
galerna_add_app(ambient_drift)
