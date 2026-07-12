cmake_minimum_required(VERSION 3.24)

# Setup language standards before project/targets.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

option(GALERNA_BUILD_TESTS "Build host-side Catch2 unit tests" OFF)
option(GALERNA_BUILD_STM32 "Build STM32 firmware target" ON)

set(CMAKE_PROJECT_NAME Galerna)

project(${CMAKE_PROJECT_NAME} LANGUAGES C CXX ASM)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "GALERNA_BUILD_STM32: ${GALERNA_BUILD_STM32}")
message(STATUS "GALERNA_BUILD_TESTS: ${GALERNA_BUILD_TESTS}")

# -----------------------------------------------------------------------------
# Galerna platform-independent C++ libraries
# -----------------------------------------------------------------------------

add_library(galerna_core INTERFACE)
target_include_directories(galerna_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(galerna_core INTERFACE cxx_std_20)

add_library(galerna_effects INTERFACE)
target_include_directories(galerna_effects INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(galerna_effects INTERFACE
    galerna_core
)

add_library(galerna_drivers INTERFACE)
target_include_directories(galerna_drivers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(galerna_drivers INTERFACE
    galerna_core
)

add_library(galerna_app INTERFACE)
target_include_directories(galerna_app INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(galerna_app INTERFACE
    galerna_core
    galerna_effects
    galerna_drivers
)

# -----------------------------------------------------------------------------
# STM32 firmware target
# -----------------------------------------------------------------------------

if(GALERNA_BUILD_STM32)
    # Main firmware executable. The CubeMX CMake file adds Core/Src/*.c,
    # startup_stm32f405xx.s, STM32 HAL/CMSIS sources, include paths and defines.
    add_executable(${CMAKE_PROJECT_NAME})
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Application/Src/swo_debug.cpp
    )

    # STM32CubeMX generated sources and libraries.
    add_subdirectory(cmake/stm32cubemx)

    add_library(galerna_platform_stm32 STATIC
        Galerna/Platform/Stm32F405/Stm32Gpio.cpp
        Galerna/Platform/Stm32F405/Stm32I2c.cpp
    )

    target_link_libraries(galerna_platform_stm32 PUBLIC
        galerna_core
        stm32cubemx
    )

    target_compile_definitions(galerna_platform_stm32 PUBLIC
        GALERNA_STM32_TARGET
    )

    add_library(application_cpp STATIC
        Application/Src/app.cpp
    )

    target_include_directories(application_cpp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Application/Inc
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(application_cpp PUBLIC
        galerna_app
        galerna_platform_stm32
        stm32cubemx
    )

    # Remove wrong libob.a library dependency when using C++ files.
    list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

    target_link_libraries(${CMAKE_PROJECT_NAME} 
        application_cpp
        stm32cubemx
        STM32_Drivers
    )
endif()

# -----------------------------------------------------------------------------
# Host unit tests
# -----------------------------------------------------------------------------

if(GALERNA_BUILD_TESTS)
    include(FetchContent)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
    )

    FetchContent_MakeAvailable(Catch2)

    enable_testing()

    add_executable(galerna_tests
        Tests/Core/test_processor_chain.cpp
        Tests/Core/test_test_tone.cpp
        Tests/Drivers/test_es8388_codec.cpp
        Tests/Drivers/test_pot_mux_4051.cpp
    )

    target_link_libraries(galerna_tests PRIVATE
        galerna_core
        galerna_effects
        galerna_drivers
        Catch2::Catch2WithMain
    )

    target_include_directories(galerna_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    include(Catch)
    catch_discover_tests(galerna_tests)
endif()
