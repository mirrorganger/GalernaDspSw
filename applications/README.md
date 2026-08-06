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
  physical chime struck by the wind, then chained into `galerna::effects::CloudReverb` (a mono,
  heavily scaled-down homage to the CloudSeed/CloudReverb algorithmic reverb, cut down hard after
  real hardware measurement — see `docs/architecture.md`'s Reverb section for the design, block
  diagrams, and the full CPU-cost tuning history) before streaming to line-out over I2S DMA
  (`Stm32I2sDuplexAudio`). Eight potentiometers control strike density, pitch spread (octave
  range), decay time, filter timbre/resonance, active voice count, and reverb mix/size; the
  status LEDs mirror the voice count in binary. Also brings up the ES8388 codec over I2C before
  starting the audio engine.

  | Control | Physical pot | Effect |
  |---|---|---|
  | Density | `POT_5` | How often voices strike: 0 = rare (up to ~6 s apart), 1 = frequent (~0.1-0.2 s apart). |
  | Spread | `POT_7` | How many octaves above the scale root a strike can land on, 0 (root only) – 1 (up to 3 octaves up). |
  | Decay | `POT_1` | How long a struck note rings out, exponential 0.2 s (short, plucky) – 3 s (long, sustained). |
  | Timbre | `POT_4` | Filter cutoff, exponential 150 Hz (dark) – 5 kHz (bright). |
  | Resonance | `POT_8` | Filter resonance, 0 (clean) – 1 (near self-oscillation). |
  | Voice count | `POT_6` | How many of the 3 voices are summed (0 = silence). Mirrored on `LED0`-`LED2` in binary. |
  | Reverb mix | `POT_3` | Dry/wet blend of the CloudReverb tail, 0 (dry) – 1 (fully wet). |
  | Reverb size | `POT_2` | How long the reverb tail sustains (late-line feedback gain), 0 (short) – 1 (long, cloudy wash). |

- **`twin_pluck/`** — Audio demo: a 4-voice gated oscillator instrument
  (`galerna::effects::TwinPluck`, driven by `TwinPluckApp`). Voices 1/2 are pot-pitched and
  gated by the two push buttons: each gates its own oscillator voice on for as long as it's
  held, then releases with an exponential decay once let go (`galerna::effects::PluckVoice`),
  pitch quantized to a pentatonic scale so the pitch pots always land on a musically consonant
  note. Voices 3/4 are fixed-pitch drones (the scale root, and a perfect fifth above it) gated
  by the two on/off switches instead -- a switch's own position is its own visual "is this
  sounding" indicator, so a switch's steady on/off state gates its drone the same way a
  button's press/release gates its voice, just latched instead of momentary. All four voices
  are summed and then chained into `galerna::effects::CloudReverb` (see
  `applications/wind_chimes/`'s entry above and `docs/architecture.md`'s Reverb section for the
  reverb design) before streaming to line-out over I2S DMA (`Stm32I2sDuplexAudio`). Seven
  potentiometers control the two pot-pitched voices plus shared release decay, filter
  timbre/resonance, and reverb mix/size; the two buttons hold voice 1/2 and their status LEDs
  mirror each voice's ringing state (LED2 unused); the two switches toggle the drone voices
  on/off (no LED, the switch position already shows it). Also brings up the ES8388 codec over
  I2C before starting the audio engine.

  | Control | Physical control | Effect |
  |---|---|---|
  | Pitch 1 | `POT_3` | Voice 1's pitch, quantized to a 3-octave pentatonic scale. |
  | Pitch 2 | `POT_5` | Voice 2's pitch, quantized to a 3-octave pentatonic scale. |
  | Decay | `POT_1` | How long a voice rings out after being released, exponential 0.2 s (short) – 3 s (long, sustained tail). Applies to all four voices. |
  | Timbre | `POT_7` | Filter cutoff, exponential 150 Hz (dark) – 5 kHz (bright). Applies to all four voices. |
  | Resonance | `POT_2` | Filter resonance, 0 (clean) – 1 (near self-oscillation). Applies to all four voices. |
  | Reverb mix | `POT_4` | Dry/wet blend of the CloudReverb tail, 0 (dry) – 1 (fully wet). |
  | Reverb size | `POT_6` | How long the reverb tail sustains (late-line feedback gain), 0 (short) – 1 (long, cloudy wash). |
  | Hold voice 1 | `BTN1` | Sustains voice 1 at its currently-set pitch for as long as held; releases on let-go. |
  | Hold voice 2 | `BTN2` | Sustains voice 2 at its currently-set pitch for as long as held; releases on let-go. |
  | Drone (root) | `SW1` | Toggles a fixed-pitch drone at the scale root on/off. |
  | Drone (fifth) | `SW2` | Toggles a fixed-pitch drone a perfect fifth above the root on/off. |

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
