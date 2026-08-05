# Galerna DSP Software

[![CI](https://github.com/mirrorganger/GalernaDspSw/actions/workflows/pr-ci.yml/badge.svg?branch=main)](https://github.com/mirrorganger/GalernaDspSw/actions/workflows/pr-ci.yml)

STM32F405-based audio DSP firmware for the Galerna board.

This repository keeps the STM32CubeMX-generated firmware project intact and adds a modern C++20 Galerna layer using concepts and template-based dependency injection.

## Important directories

- `firmware_core/` — MCU bring-up/init code, IRQ table, startup file (`startup_stm32f405xx.s`) and linker script (`STM32F405XX_FLASH.ld`), originally scaffolded by STM32CubeMX, now hand-maintained, with its own `CMakeLists.txt`.
- `drivers/` — STM32 HAL/CMSIS files, with its own `CMakeLists.txt`.
- `galerna/` — Galerna C++ libraries (`app`, `core`, `drivers`, `effects`, `hal`, `platform`), each with its own `CMakeLists.txt` and `include/`/`src`(`platform` only)/`tests` layout.
- `applications/` — one independently-buildable, independently-flashable firmware app per subdirectory (e.g. `thx_deep_note/`, `pot_blink/`), plus `applications/common/` for the C/C++ bridge called from CubeMX `main.c`, shared by every app.
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
