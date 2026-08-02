# Galerna DSP Software

[![CI](https://github.com/mirrorganger/GalernaDspSw/actions/workflows/pr-ci.yml/badge.svg?branch=main)](https://github.com/mirrorganger/GalernaDspSw/actions/workflows/pr-ci.yml)

STM32F405-based audio DSP firmware for the Galerna board.

This repository keeps the STM32CubeMX-generated firmware project intact and adds a modern C++20 Galerna layer using concepts and template-based dependency injection.

## Important directories

- `Core/` — Application/init code, originally scaffolded by STM32CubeMX, now hand-maintained.
- `Drivers/` — STM32 HAL/CMSIS files.
- `cmake/stm32cubemx/` — CMake integration, originally scaffolded by STM32CubeMX, now hand-maintained.
- `startup_stm32f405xx.s` — STM32 startup file.
- `STM32F405XX_FLASH.ld` — STM32 linker script.
- `Application/` — C/C++ bridge called from CubeMX `main.c`.
- `Galerna/` — Galerna C++ app, drivers, effects, concepts and STM32 platform adapters.
- `Tests/` — host-side Catch2 tests.
- `.devcontainer/`, `Dockerfile`, `compose.yaml` — reproducible VSCode/Docker development environment.

## Build firmware

```bash
cmake --preset Debug
cmake --build --preset Debug
```

```bash
cmake --preset Release
cmake --build --preset Release
```

## Run host tests

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

## Docker / devcontainer

```bash
docker compose build galerna-build
docker compose run --rm galerna-build bash
```

Inside the container, use the same CMake presets.
