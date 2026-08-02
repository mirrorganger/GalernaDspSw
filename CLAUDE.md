# CLAUDE.md

Guidance for AI agents working in this repository.

## What this is

Firmware for the Galerna DSP board (STM32F405-based guitar/line audio effects unit).
Hardware project (KiCad, `docs/Design.md`) lives in the sibling repo
`mirrorganger/GalernaDsp`. See `README.md` and `docs/architecture.md` for directory layout
and build commands.

## Coding conventions

- Use modern C++ (C++20) for all project-owned code under `Galerna/`, `Application/`, and
  `Tests/`. Prefer `std::span`, `std::array`, RAII, and `constexpr` over raw pointers, manual
  loops over C arrays, and preprocessor tricks. No exceptions/RTTI (the STM32 build compiles
  with `-fno-exceptions -fno-rtti`), so keep new code compatible with that.
- Use modern techniques for HAL-facing code: C++20 concepts (`Galerna/Hal/*Concept.hpp`) plus
  template-based dependency injection, not virtual interfaces. A driver should be a class
  template constrained on a concept (`hal::Gpio`, `hal::Adc`, `hal::I2cBus`, ...), so it can be
  instantiated with either a real `Stm32*` adapter or a host-side fake.
- Everything should be testable on the host (Catch2, `Tests/`) **except** low-level platform
  code in `Galerna/Platform/Stm32F405/` (thin wrappers directly over STM32 HAL calls — these
  have no meaningful logic to unit test and aren't compiled for the host target). If you add
  logic that isn't a direct HAL passthrough, it belongs in a platform-independent class
  (`Galerna/Core`, `Galerna/Drivers`, `Galerna/Effects`, `Galerna/App`) that takes the platform
  type as a template parameter, so it can be tested against a `Tests/Fakes/*` fake instead.
- Match existing driver shape: constructor takes the dependencies by reference, a small public
  API (`read`/`write`/`set`/`get`), concept-checked template parameters, `_memberName` naming.

## Working in this repo

- `Core/`, `Drivers/`, `cmake/stm32cubemx/`, `startup_stm32f405xx.s`, `STM32F405XX_FLASH.ld`
  were originally scaffolded by STM32CubeMX but are no longer regenerated — the board's
  pinout is finalized, and `Galerna.ioc` has been retired to `docs/cubemx/` as a frozen
  reference snapshot (see `docs/cubemx/README.md`). These files are now hand-maintained
  firmware source like any other file in the repo: edit them directly when needed (e.g. add
  a peripheral by mirroring an existing `MX_*_Init` function and its call site in
  `Core/Src/main.c`), no CubeMX GUI/CLI round-trip required or expected.
- Verify changes with the narrowest relevant build/test command before reporting completion:
  `cmake --build --preset host-debug && ctest --preset host-debug` for project-owned C++ logic,
  the STM32 `Debug`/`Release` presets only when platform code changed (these need the
  Docker/devcontainer toolchain — see `README.md`).
