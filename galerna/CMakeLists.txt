add_subdirectory(hal)
add_subdirectory(core)
add_subdirectory(effects)
add_subdirectory(drivers)
add_subdirectory(app)

if(GALERNA_BUILD_STM32)
    add_subdirectory(platform)
endif()
