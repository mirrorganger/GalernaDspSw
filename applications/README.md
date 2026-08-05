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

  | Control | Physical pot | Effect |
  |---|---|---|
  | Pitch | `POT_5` | Glides all active voices from a low scattered cluster (0) to their target chord (1). |
  | Pitch shift | `POT_7` | Additively offsets the whole glide range up to +500 Hz. |
  | Timbre | `POT_4` | Filter cutoff, exponential 150 Hz (dark) – 5 kHz (bright). |
  | Resonance | `POT_8` | Filter resonance, 0 (clean) – 1 (near self-oscillation). |
  | Voice count | `POT_6` | How many of the 7 voices are summed (0 = silence). Mirrored on `LED0`-`LED2` in binary. |

- **`wind_chimes/`** — Audio demo: a generative ambient synth (`galerna::effects::WindChimes`,
  driven by `WindChimesApp`) where a bank of independently-scheduled `WindChimeVoice` "strikers"
  wait a randomized interval, then ring out a note from a pentatonic scale and decay, like a
  physical chime struck by the wind — streamed to line-out over I2S DMA
  (`Stm32I2sDuplexAudio`). Six potentiometers control strike density, pitch spread (octave
  range), decay time, filter timbre/resonance, and active voice count; the status LEDs mirror
  the voice count in binary. Also brings up the ES8388 codec over I2C before starting the audio
  engine.

  | Control | Physical pot | Effect |
  |---|---|---|
  | Density | `POT_5` | How often voices strike: 0 = rare (up to ~6 s apart), 1 = frequent (~0.1-0.2 s apart). |
  | Spread | `POT_7` | How many octaves above the scale root a strike can land on, 0 (root only) – 1 (up to 3 octaves up). |
  | Decay | `POT_1` | How long a struck note rings out, exponential 0.2 s (short, plucky) – 3 s (long, sustained). |
  | Timbre | `POT_4` | Filter cutoff, exponential 150 Hz (dark) – 5 kHz (bright). |
  | Resonance | `POT_8` | Filter resonance, 0 (clean) – 1 (near self-oscillation). |
  | Voice count | `POT_6` | How many of the 8 voices are summed (0 = silence). Mirrored on `LED0`-`LED2` in binary. |

- **`pot_blink/`** — Minimal demo (`galerna::app::GalernaApp`): three status LEDs blink at
  rates set by three potentiometers, with buttons/switches read alongside. No audio path;
  useful as a smoke test for GPIO/ADC/mux wiring independent of the codec/I2S path.

  | Control | Physical control | Effect |
  |---|---|---|
  | `LED0` rate | `POT_1` | Blink frequency, 0.5 Hz (pot at 0) – 8 Hz (pot at max). |
  | `LED1` rate | `POT_2` | Same range as `LED0`. |
  | `LED2` rate | `POT_3` | Same range as `LED0`. |
  | Pause | `BTN1` or `BTN2` (either, held) | Freezes all three LEDs' current on/off state. |
  | `LED0` enable | `SW1` | LED0 forced off while switched off; `LED1`/`LED2` unaffected. |
  | `LED1` enable | `SW2` | LED1 forced off while switched off; `LED0`/`LED2` unaffected. |

- **`common/`** — Glue shared by every app: `app.h` (the `App_Init`/`App_Tick` declarations
  called from `firmware_core/src/main.c`) and `SwoDebug.cpp` (SWO `printf` plumbing).
