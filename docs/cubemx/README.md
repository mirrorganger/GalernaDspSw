# CubeMX reference snapshot

`Galerna.ioc` and `.mxproject` here are a frozen snapshot of the last STM32CubeMX
regeneration (based off commit `2a7dca3`, 2026-08-02), kept for reference only.

`firmware_core/` (originally CubeMX's `Core/`, renamed to avoid colliding with
`galerna/core`, now with its own `CMakeLists.txt`) and `drivers/` (originally
`Drivers/`, also with its own `CMakeLists.txt`) are **no longer regenerated** from
this file — they are hand-maintained firmware source like any other file in the
repo. See `CLAUDE.md` and `docs/architecture.md` for how to hand-edit them (e.g.
adding a peripheral by mirroring an existing `MX_*_Init` function in
`firmware_core/src/main.c`).

Only reopen `Galerna.ioc` in CubeMX for a deliberate, reviewed hardware-revision
migration — never generate code directly into the live tree. If you do, treat the
regeneration output as a proposed diff: review it in full against the current
`firmware_core/`/`drivers/` before accepting any part of it (matching it back up
against CubeMX's original `Core/`/`Drivers/` naming), since CubeMX will silently
regenerate files outside its `USER CODE` markers (this has previously reverted
C++ build settings in the CMake integration and dropped application code from
`Core/Src/main.c`).
