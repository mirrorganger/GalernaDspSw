# Galerna DSP Software Architecture

This project keeps the STM32CubeMX generated firmware structure and adds a modern C++ Galerna layer.

## CubeMX-owned files

These files remain part of the real firmware build:

- `Core/`
- `Drivers/`
- `cmake/stm32cubemx/`
- `startup_stm32f405xx.s`
- `STM32F405XX_FLASH.ld`
- `cmake/gcc-arm-none-eabi.cmake`

## Galerna-owned files

- `Application/`: C-compatible bridge called by `Core/Src/main.c`.
- `Galerna/Core/`: platform-independent DSP primitives.
- `Galerna/Drivers/`: device drivers using concepts and template dependency injection.
- `Galerna/Effects/`: DSP effects.
- `Galerna/App/`: product-level Galerna logic.
- `Galerna/Platform/Stm32F405/`: STM32-specific adapters over HAL.
- `Tests/`: host-side Catch2 tests.

## Build commands

STM32 firmware:

```bash
cmake --preset Debug
cmake --build --preset Debug

cmake --preset Release
cmake --build --preset Release
```

Host tests:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```
