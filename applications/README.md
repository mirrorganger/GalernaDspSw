# Applications

Each subdirectory here is one independently-buildable, independently-flashable firmware
app (see `docs/architecture.md` for how `galerna_add_app()` wires an app's `app.cpp` into
its own `.elf`). All apps share the same CubeMX bring-up, HAL drivers, and `galerna::*`
libraries — only `app.cpp` (and any app-specific classes next to it) differs.

- **`thx_deep_note/`** — Audio demo: a THX "Deep Note"-style synth (`galerna::effects::ThxDeepNote`,
  driven by `ThxDeepNoteApp`) generated continuously and streamed to line-out over I2S DMA
  (`Stm32I2sDuplexAudio`). Five potentiometers (via `PotMux4051`) control pitch, pitch shift,
  timbre, resonance, and active voice count; the status LEDs mirror the voice count in binary.
  Also brings up the ES8388 codec over I2C before starting the audio engine.
- **`pot_blink/`** — Minimal demo (`galerna::app::GalernaApp`): three status LEDs blink at
  rates set by three potentiometers, with buttons/switches read alongside. No audio path;
  useful as a smoke test for GPIO/ADC/mux wiring independent of the codec/I2S path.

- **`common/`** — Glue shared by every app: `app.h` (the `App_Init`/`App_Tick` declarations
  called from `firmware_core/src/main.c`) and `SwoDebug.cpp` (SWO `printf` plumbing).
