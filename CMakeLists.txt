cmake_minimum_required(VERSION 3.24)

# Setup language standards before project/targets.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

option(GALERNA_BUILD_TESTS "Build host-side Catch2 unit tests" OFF)
option(GALERNA_BUILD_STM32 "Build STM32 firmware target" ON)

project(Galerna LANGUAGES C CXX ASM)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "GALERNA_BUILD_STM32: ${GALERNA_BUILD_STM32}")
message(STATUS "GALERNA_BUILD_TESTS: ${GALERNA_BUILD_TESTS}")

if(GALERNA_BUILD_TESTS)
    include(FetchContent)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
    )

    FetchContent_MakeAvailable(Catch2)

    enable_testing()
    include(Catch)

    # Stub target: each galerna/<lib>/CMakeLists.txt appends its own test sources via
    # target_sources(galerna_tests ...) from its own directory scope as galerna/ is added
    # below; linked against every galerna::* library once they all exist, further down.
    add_executable(galerna_tests)
    target_link_libraries(galerna_tests PRIVATE Catch2::Catch2WithMain)
endif()

if(GALERNA_BUILD_STM32)
    # STM32CubeMX generated sources and libraries (stm32cubemx, STM32_Drivers) -- needed
    # before add_subdirectory(galerna) since galerna/platform links against stm32cubemx.
    add_subdirectory(cmake/stm32cubemx)
endif()

add_subdirectory(galerna)

if(GALERNA_BUILD_TESTS)
    target_link_libraries(galerna_tests PRIVATE
        galerna::core
        galerna::effects
        galerna::drivers
        galerna::app
        galerna::hal
        galerna::hal::fakes
    )
    catch_discover_tests(galerna_tests)
endif()

if(GALERNA_BUILD_STM32)
    add_subdirectory(applications)
endif()
