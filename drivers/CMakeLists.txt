# Vendor STM32 HAL/CMSIS sources -- untouched internal structure, see docs/cubemx/README.md.

# Interface library for CubeMX-generated include paths and preprocessor defines.
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32F4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32F4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS/Include
)
target_compile_definitions(stm32cubemx INTERFACE
    USE_HAL_DRIVER
    STM32F405xx
    $<$<CONFIG:Debug>:DEBUG>
)
# Every HAL source includes stm32f4xx_hal_conf.h, which lives in firmware_core/inc.
target_link_libraries(stm32cubemx INTERFACE firmware_core)

# STM32 HAL/LL static library. system_stm32f4xx.c lives in firmware_core/src (that's
# where CubeMX puts it, alongside main.c) even though it's compiled into this vendor
# driver library, not the per-app bring-up sources -- see galerna_configure_cubemx_target()
# in firmware_core/CMakeLists.txt for those.
add_library(STM32_Drivers STATIC
    ${CMAKE_SOURCE_DIR}/firmware_core/src/system_stm32f4xx.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2s.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2s_ex.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_sdmmc.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_mmc.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c
    STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c
)
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)
