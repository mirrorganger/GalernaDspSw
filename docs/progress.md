# Bring-up Findings Log

Structured log of hardware/firmware bring-up investigations: symptom, how it was
diagnosed, the actual root cause, and the fix. Kept separate from `architecture.md`
(which documents the codebase as it stands) because these entries are historical
records of a specific problem and how it was resolved.

## Entry format

```
## <date> — <area>

**Symptom:** what was observed.

**Investigation:** steps taken, including dead ends.

**Root cause:** what was actually wrong.

**Fix:** files changed and what changed.

**Verification:** how the fix was confirmed.
```

---

## 2026-07-23 — Potentiometer mux (U7, CD4051BM) readings uncorrelated with pots

**Symptom:** With POT_1, POT_2, POT_3 soldered (the rest of the 8-pot mux left
unpopulated), `printPotValues()` SWO output changed continuously but the values
didn't track turning any specific pot, and never approached the ADC's rail
values (0 / 4095).

**Investigation:**

1. Reviewed `Galerna/Drivers/PotMux4051.hpp` and `Galerna/Platform/Stm32F405/Stm32Adc.cpp`
   — `read(muxChannel)` correctly selects the mux lines then does a single-shot
   ADC conversion; no obvious logic bug.
2. Needed to know which physical pot corresponds to which mux channel argument.
   No exported netlist existed and `kicad-cli` wasn't found on `PATH`, so the
   sibling HW repo's `input_control.kicad_sch` was parsed by hand (custom
   Python S-expression parser) to resolve symbol/pin absolute positions and
   trace wires/net-labels.
   - **Dead end:** the manual parse produced a wrong channel→pot map, and
     additionally (mis-)concluded that U7's VDD and select-C pins were swapped
     and that COM_OUT/IN, A, B, and INH were floating — a serious-sounding
     hardware defect that turned out to be a bug in the hand-rolled mirrored-
     symbol pin-position transform, not a real problem on the board.
3. Found `kicad-cli` was in fact already available via the `org.kicad.KiCad`
   Flatpak (`flatpak run --command=kicad-cli org.kicad.KiCad ...`) — no install
   needed. Ran the authoritative tools instead of hand parsing:
   - `kicad-cli sch erc --severity-all --exit-code-violations GalernaDsp.kicad_sch`
     → only 1 warning (unrelated `lib_symbol_mismatch`), zero connectivity errors.
   - `kicad-cli sch export netlist --format kicadxml -o netlist.xml GalernaDsp.kicad_sch`
     → confirmed U7 is wired correctly: `VDD→+3V3`, `INH/VSS/VEE→GND`,
     `A/B/C→POT_MUX_SEL_0/1/2`, `COM_OUT/IN→R34(1k)→POT_MUX_ADC(PA0)` with
     `C52` (100 nF) filtering that node. The earlier "swapped/floating pins"
     claim was wrong.
   - Re-derived the true mux-channel → pot mapping from the netlist (see table
     below) — different from, and superseding, the earlier hand-derived one.
4. With the corrected mapping, POT_1/2/3 still didn't look right at first
   glance — traced to `R34` (1k) + `C52` (100 nF) forming a ~100 µs RC filter
   on the shared ADC node. The firmware switched mux channels and started the
   ADC conversion immediately (`ADC_SAMPLETIME_3CYCLES`, no settling delay),
   so each reading was still influenced by the *previous* channel's voltage.

**Root cause:** two compounding issues, one investigative and one real:

- The channel→pot mapping being used for verification was wrong (artifact of
  hand-parsing the schematic instead of using `kicad-cli`), so the wrong
  print-output slots were being watched.
- Real firmware bug: no settling delay between selecting a new mux channel and
  starting the ADC conversion, so readings lagged/blended between channels.

**Fix:**

- `Galerna/Platform/Stm32F405/Stm32Adc.cpp`: added `HAL_Delay(1U)` between
  `HAL_ADC_ConfigChannel` and `HAL_ADC_Start` to let the mux + RC filter settle
  before sampling.
- `Application/Src/app.cpp`: `printPotValues()` now labels each reading with
  its physical pot reference (`POT_1`..`POT_8`) via a netlist-verified lookup
  table, instead of the raw mux channel index, so the output is self-
  explanatory and can't cause the same mapping confusion again.

**Verification:** STM32 `Debug` preset built clean and host `host-debug`
Catch2 suite passed 8/8, both via the `galerna-build` Docker container. User
confirmed on real hardware that POT_1/2/3 now track cleanly.

**Mux channel → physical pot** (from `kicad-cli sch export netlist`, U7 Y-pin
nets on `input_control.kicad_sch`):

| Channel | Pot |
|---|---|
| 0 | POT_3 |
| 1 | POT_5 |
| 2 | POT_1 |
| 3 | POT_7 |
| 4 | POT_2 |
| 5 | POT_4 |
| 6 | POT_6 |
| 7 | POT_8 |

**Process note:** when a schematic-connectivity question comes up, check for
`kicad-cli` as a Flatpak (`flatpak run --command=kicad-cli org.kicad.KiCad`)
before hand-parsing `.kicad_sch` S-expressions. Manual parsing of mirrored/
rotated symbol pin transforms is easy to get subtly wrong and will produce
confident-sounding but incorrect connectivity claims; `kicad-cli sch erc` and
`sch export netlist` are authoritative and cheap to run.

---

## 2026-07-23 — Push buttons (BTN1/BTN2) configured with floating inputs

**Symptom:** none observed yet on hardware — found while wiring BTN1/BTN2 and
SW1/SW2 into `GalernaApp` (LED pause/enable controls), before any real-hardware
test of the buttons.

**Investigation:**

1. `kicad-cli sch export netlist` (same method as the pot mux entry above) on
   the sibling `~/wk/kicad/GalernaDsp` repo to get the authoritative
   component→net→MCU-pin mapping for BTN1, BTN2, SW1, SW2, since none of them
   had been used in firmware yet.
   - Found the silkscreen refs don't match the net names: BTN1 → net
     `PUSH_BTN_0` → `PC1`; BTN2 → net `PUSH_BTN_1` → `PC0`. SW1 → net `SW_2` →
     `PB4`; SW2 → net `SW_3` → `PB5`.
   - The sibling repo's `cubeMx/Galerna.ioc` has stale/incorrect
     `GPIO_Label`/`GPIO_PuPd` text for these same pins (e.g. it labels `PC0` as
     `SW_1` and gives `PB4` a `PUSH_BTN` pull-up) — doesn't match this fw
     repo's already-generated `Core/Inc/main.h` / `Core/Src/main.c`, which use
     the correct net-derived names. Treated the compiled `Core/` files as
     ground truth, not the `.ioc`, same lesson as the pot mux entry (don't
     trust a document, verify the actual generated/connected wiring).
2. Checked the netlist for pull resistors on each net: BTN1/BTN2's nets have
   only the button and the MCU pin — no external pull. SW1/SW2's nets have the
   switch's common pin plus the MCU pin, but the switch's other two pins go
   directly to `+3V3` and `GND` (it's an SPDT slide switch), so no pull is
   needed there — the switch itself always drives the line.
3. Cross-checked `Core/Src/main.c`'s `MX_GPIO_Init`: the `PUSH_BTN_1_Pin|PUSH_BTN_0_Pin`
   block was configured `GPIO_MODE_INPUT` with `Pull = GPIO_NOPULL`. Combined
   with finding (2), the buttons would float to an undefined level whenever
   released.

**Root cause:** `Core/Src/main.c`'s `MX_GPIO_Init` had the push-button GPIOs
configured with no pull, and the board provides no external pull resistor for
them — the input floats when the button isn't pressed.

**Fix:**

- `Core/Src/main.c`: `PUSH_BTN_1_Pin|PUSH_BTN_0_Pin` `GPIO_InitStruct.Pull`
  changed from `GPIO_NOPULL` to `GPIO_PULLUP`. With pull-up and press-to-GND
  wiring, a pressed button now reads `false` (active-low).
- `Galerna/App/GalernaApp.hpp` / `Application/Src/app.cpp`: BTN1/BTN2 wired in
  as the LED-pause inputs, SW1/SW2 as the LED0/LED1 enable switches, using the
  netlist-verified pin mapping above (documented in a comment in `app.cpp` so
  the silkscreen-vs-net mismatch doesn't cause the same confusion again).

**Verification:** STM32 `Debug` preset built clean and host `host-debug`
Catch2 suite passed 16/16, both via the `galerna-build` Docker container.
**Not yet verified on real hardware** — pending confirmation that BTN1/BTN2
reliably pause LED blinking without spurious floating-pin triggers, and that
SW1/SW2 enable/disable LED0/LED1 as expected.

---

## 2026-08-03 — Full-duplex DMA audio (ES8388) silent despite a provably-correct digital pipeline

**Symptom:** After adding DMA-driven full-duplex I2S audio (`HAL_I2SEx_TransmitReceive_DMA`,
`Stm32I2sDuplexAudio`), the SWO bring-up log always printed cleanly through "I2S duplex audio:
started", but line-out/headphone-out was completely silent — first with a real line-in ->
`Bypass` passthrough, then with a self-generated diagnostic test tone that doesn't depend on
line-in at all. The exact same codec (ES8388) and I2S2/I2S2ext wiring had produced audible
output before, via a blocking `HAL_I2S_Transmit()` call (dead code, no DMA) that predates this
feature.

**Investigation:** Several real bugs were found and fixed along the way, but none of them turned
out to be the actual cause of the silence — each was a legitimate improvement that still left the
board silent, which is what eventually pointed at something outside the firmware entirely.

1. **First real bug (fixed, but not the cause of silence):** the three
   `HAL_I2SEx_TxRxHalfCpltCallback`/`HAL_I2SEx_TxRxCpltCallback`/`HAL_I2S_ErrorCallback`
   overrides were originally `inline` functions in `Stm32I2sDuplexAudio.hpp`. Even after making
   them ordinary (non-`inline`) functions in a dedicated `Stm32I2sDuplexAudioCallbacks.cpp`, `nm`
   on the final `.elf` still showed them as weak (`W`), not strong (`T`). Root cause: that file
   lived inside `galerna_platform_stm32`, a **static archive**. HAL's own
   `stm32f4xx_hal_i2s_ex.c.o` both *calls* and `__weak`-*defines* the same callback names, so by
   the time the linker reached our archive, the symbol was already "resolved" (weakly) and our
   archive member was never pulled in — static archives only pull in members to satisfy symbols
   still undefined at that point, they don't retroactively prefer a stronger definition found
   later. **Fix:** compile `Stm32I2sDuplexAudioCallbacks.cpp` directly into the `Galerna`
   executable target (`target_sources`), not into the static library, so it's unconditionally
   linked. Verified via `arm-none-eabi-nm build/Debug/Galerna.elf` that the three symbols became
   `T` (strong) after the fix.
2. Added `Stm32I2sDuplexAudio::callCount()`/`errorCount()` (now a permanent diagnostic, printed
   alongside `printPotValues()`) to get an authoritative answer on whether the DMA callback chain
   was actually firing, instead of inferring it from SWO corruption. It was: `callCount` climbed
   steadily with `errorCount` always 0.
3. Suspected `std::sin()` (floating point) inside the DMA ISR next, since the LED blink loop
   appeared to stall after the linker fix. Swapped the diagnostic tone generator to pure
   integer/wavetable math (`galerna::core::TestTone`, no `ProcessorChain`/`AudioBuffer`/float
   involved at all) to isolate it. Status LEDs kept blinking throughout — the system was never
   actually hung — but line-out was still silent, ruling this out.
4. A/B tested the codec register script: temporarily restored the exact old DAC-only script
   (`configureForI2sDacPlaybackOnly`, since deleted) that had been confirmed audible via blocking
   transmit, keeping the new DMA tone generator. Still silent — ruled out
   `configureForI2sDuplex()`'s new ADC-path/mixer/`0x2B` additions as the cause.
5. With hardware physically connected, flashed and inspected the board directly (`openocd` +
   `arm-none-eabi-gdb`, SWD): read `SPI2`/`I2S2ext` `I2SCFGR`/`CR2`/`SR` live — both enabled,
   correct Master-TX/Slave-RX mode, correct Philips/16-bit/clock-polarity bits, `BSY=1`,
   `TXDMAEN`/`RXDMAEN=1`, zero error flags. Read `DMA1_Stream3`/`Stream4` `CR` live — correct
   channel (3/0), direction, circular mode, increment/size settings, enabled, zero errors.
   Dumped `audioEngine._txBuffer` from RAM directly — a mathematically perfect sine wave matching
   `TestTone`'s wavetable exactly. Attempted an I2C register readback via `HAL_I2C_Mem_Read`
   through GDB's `call` to confirm the codec's own register state, but that function had been
   linked out by the same static-archive-member-pulling behavior as bug #1 (never explored
   further — not needed once the real cause was found).

**Root cause:** three separate, compounding issues, found in sequence as each prior one was ruled
out — none of them were in the code paths originally suspected (DMA config, codec register
script):

1. **Codec latch state (the original silence, diagnostic tone included):** the board had been
   reflashed and soft-reset (via `openocd ... reset`, MCU-only, no power removed) many times in a
   row while debugging. The ES8388's I2C control-port writes always ACKed successfully
   throughout, but its internal DEM/state-machine apparently latched into a bad state at some
   point that an MCU-only reset does not clear (the codec is a separate I2C-controlled chip with
   its own internal state, not reset by `NRST` alone). A full power cycle immediately fixed it —
   confirmed audible with the diagnostic tone.
2. **Debug build (`-O0`) couldn't keep up with the real-time ISR budget:** with the diagnostic
   tone reverted back to the real `ProcessorChain<Bypass>` + `DuplexAudioBlockProcessor` pipeline
   (which does int16<->float conversion, unlike the diagnostic tone), the board appeared to hang
   — no LED blink, no audio. Live SWD inspection (`arm-none-eabi-gdb`, `bt full`, `CFSR`/`HFSR`)
   showed **zero fault registers set** and a completely coherent call stack: the DMA ISR had
   legitimately interrupted the main loop mid-`GalernaApp::tick()`. Not a crash — CPU starvation.
   `SystemCoreClock` is only 24 MHz, and at `-O0` (STM32 `Debug` preset) the audio ISR's
   processing was slow enough, relative to its ~1.3 ms real interrupt period (measured via
   `Stm32I2sDuplexAudio::callCount()`), to consume effectively all CPU time, starving the main
   loop to the point of looking hung. **Fix: use the `Release` preset (`-Os`) for real hardware
   audio testing, not `Debug`.** Flash size dropped from 56 KB to 19 KB between the two builds for
   the same source, illustrating how much unoptimized overhead was in play. Confirmed via SWO
   that `Pots:`/`callCount` resumed printing normally on the `Release` build.
3. **`docs/Design.md`'s BOM table has the wrong jack order** (sibling `~/wk/kicad/GalernaDsp`
   repo, not this one): it lists "J2, J3, J4 → Line in, line out, headphone out", but the
   schematic's own component values (`kicad-cli sch export netlist`) say otherwise — `J2` =
   `HEADPHONES` (net `HEADPHONE_OUT_L/R`), `J3` = `AUDIO_IN` (net `AUDIO_L_IN`/`AUDIO_R_IN`,
   **the actual line-in jack**), `J4` = `AUDIO_OUT` (line-out). Going by the BOM table's stated
   order plugs the phone into the headphone-*output* jack instead — a perfectly working audio
   pipeline with genuinely nothing to pass through. Same class of doc/silkscreen-vs-schematic
   mismatch as the pot-mux and BTN/SW entries above; not yet fixed in `Design.md` itself.

**Fix:** `Galerna/Platform/Stm32F405/Stm32I2sDuplexAudioCallbacks.cpp` moved to
`target_sources(${CMAKE_PROJECT_NAME} ...)` in `CMakeLists.txt` (not `galerna_platform_stm32`) —
a real bug, fixed, but not the cause of the silence (see investigation #1 above).
`Stm32I2sDuplexAudio::callCount()`/`errorCount()` kept as permanent diagnostics. No other
firmware change was needed; the fixes were procedural (power-cycle, build with `Release`, plug
into `J3`).

**Verification:** STM32 `Debug` **and** `Release` presets, plus host `host-debug` Catch2 suite
(18/18), all build/pass clean via the `galerna-build` Docker container. `arm-none-eabi-nm`
confirmed the HAL callback overrides are strong (`T`) symbols in the final `.elf`. Live SWD
register/memory inspection confirmed the entire STM32-side digital audio pipeline (I2S
peripherals, both DMA streams, the actual tone data in RAM) is correct. **User-confirmed on real
hardware:** full-duplex line-in (`J3`) -> `Bypass` -> line-out/headphone-out audio passthrough
works, with LEDs blinking normally (`Release` build, after a full power cycle).

**Process notes for next time:**
- If ES8388 audio goes mysteriously silent after a lot of flash/reset cycling during a debug
  session despite everything checking out in firmware, **fully power-cycle the board** before
  assuming it's a code bug — an MCU-only reset is not guaranteed to reset the codec's internal
  state.
- **Always use the `Release` preset for real-time audio testing on hardware.** The `Debug`
  preset's `-O0` build is for logic/build verification, not for judging whether audio-path timing
  actually works — it can look identical to a hard crash (no fault registers, but the main loop
  starves) if the ISR can't keep up.
- The line-in jack is **J3**, not J2 (`Design.md`'s BOM table has this wrong — worth fixing there
  too, out of scope for this repo).
- ~~Follow-up, not yet investigated: `callCount()` climbs at roughly 790-800/s, appreciably faster
  than the ~500/s implied by the nominal 32 kHz sample rate and 64-frame half-buffer size.~~
  **Resolved 2026-08-05** (see that entry below): computing the true I2S rate directly from live
  `RCC->PLLI2SCFGR`/`SPI2->I2SPR` register reads gives Fs ≈ 32,552 Hz — very close to the nominal
  32 kHz assumed elsewhere, implying a ceiling of ≈509 halves/s, not 790-800. The `callCount`
  figure recorded here was almost certainly a stale/mismeasured artifact from that session, not a
  real clock discrepancy; `Application/Src/app.cpp`'s `audioSampleRateHz` now uses the exact
  register-derived value instead of the nominal 32 kHz (a ~1.7% correction).

---

## 2026-08-05 — THX Deep Note-style synth: 15-voice reference design overran the audio ISR

**Symptom:** Real line-in capture is dead at the hardware level (physical `PB14`/`ASDOUT` stuck
high — root-caused in an earlier session to a likely solder-joint defect, needs a multimeter to
pin down further; out of scope for this entry). Rather than block on that, ported a "Deep
Note"-style synth (informally similar to the classical THX startup sound) from the sibling
`galernaDaisy` project's `ThxSeedApp` (Electrosmith Daisy Seed, 480 MHz) as a standalone,
output-only demo effect: `Galerna/Core/WavetableOscillator.hpp` (additive-harmonic sawtooth
wavetable), `Galerna/Core/ThxVoice.hpp` (one glide-and-vibrato voice), `Galerna/Effects/ThxDeepNote.hpp`
(a bank of voices summed and tone-shaped, dropped into the existing `ProcessorChain` in place of
`Bypass` — no DMA/codec/platform changes needed, since it just overwrites the `AudioBuffer` instead
of reading the broken rx data). Ported faithfully at first, including the reference's
`NR_VOICES = 15`. Host tests (30/30) and both STM32 `Debug`/`Release` builds were clean, but real
hardware told a different story.

**Investigation:**

1. Flashed the `Release` build (per the `Release`-not-`Debug` rule from the entry above) and
   captured SWO: codec bring-up and `I2S duplex audio: started` all printed correctly, but the
   periodic `Pots:`/`Audio DMA callCount` diagnostic **never printed again** afterward, across
   repeated multi-second capture windows.
2. `arm-none-eabi-gdb` live inspection (`bt full`) caught the CPU mid-`ThxDeepNote::processBlock`
   (voice 4 of 15, frame 3 of 64) with the main loop's `App_Tick()` visibly preempted from inside
   an ADC-read `HAL_Delay()` — consistent with an ISR that isn't finishing inside its budget.
3. Measured `Stm32I2sDuplexAudio::callCount()` throughput directly: halt, read the counter,
   `monitor resume`, wait ~1 s of real target time, halt, read again. **208 calls/s**, against a
   ~509/s ceiling implied by the (now correctly known, see the resolved follow-up above) true
   I2S rate and the 64-frame half-buffer — only **~41% of real-time**. `errorCount` stayed 0
   throughout: DMA doesn't fault when software falls behind, it just keeps transmitting stale
   buffer content, so the failure mode is glitchy/torn audio, not a crash or a logged error.
4. First mitigation: decimated `ThxVoice`'s per-sample LFO evaluation to once every 4 samples
   (still >100x oversampled relative to the LFO's 5-30 Hz rate, inaudible) and cut voice count
   15 → 8. Re-measured: **472 calls/s (~94%)** — better, but still short.
5. Cut voices 8 → 6: **~526 calls/s**, above the ceiling. Re-tested with the LFO decimation
   stride widened 4 → 8 (expecting a further easy win): **no measurable change** (~526 calls/s
   either way) — evidence that per-voice oscillator cost, not the LFO, was now the dominant
   remaining cost, so widening LFO decimation further was reverted (no benefit, and stride 4 is
   marginally smoother). Cut voices 6 → 5 as a result: also **~520-527 calls/s**, i.e. voice count
   had stopped moving the needle much either, both configurations apparently near a plateau.
6. Because `callCount` saturates at the physical interrupt rate once the ISR is fast enough to
   keep up (the DMA fires the half-complete interrupt at a fixed hardware cadence regardless of
   software speed), a saturated reading alone can't distinguish "healthy, ISR finishes early" from
   "barely keeping up." Cross-checked with a statistical sample instead: 20 rapid
   halt/`bt 1`/resume cycles at the 5-voice configuration landed the CPU inside the audio
   pipeline (`ThxDeepNote`/`ThxVoice`/`WavetableOscillator`/`DuplexAudioBlockProcessor`) in 14/20
   samples and in the main loop (`HAL_Delay`/ADC polling) in the other 6 — busy, but clearly not
   pinned inside the ISR 100% of the time the way the genuinely-overrunning 15-voice case was
   (2/2 samples caught mid-ISR in the very first backtrace check). Consistent with the ISR
   completing comfortably inside its ~2 ms/half budget, not saturating it.
7. While investigating the true hardware ceiling, resolved the "resolved 2026-08-05" note above:
   live `RCC->PLLI2SCFGR`/`SPI2->I2SPR` register reads gave the exact I2S rate (≈32,552 Hz, not
   the nominal 32 kHz `app.cpp` had assumed) — corrected `audioSampleRateHz` to the precise
   register-derived value while in there.

**Root cause:** `NR_VOICES = 15` (2 wavetable-oscillator evaluations per voice per sample: audio
osc + LFO) was tuned for the reference project's much faster board (Daisy Seed, 480 MHz
Cortex-M7) and is too much floating-point/function-call work for this board's 24 MHz Cortex-M4F
within the audio ISR's ~2 ms/half real-time budget, even at `-Os`.

**Fix:** `Galerna/Core/ThxVoice.hpp` decimates its LFO to once every `lfoUpdateStride = 4` samples
(compensating the LFO's own oscillator init rate so its real-time frequency stays correct despite
being evaluated less often). `Galerna/Effects/ThxDeepNote.hpp`'s `voiceCount` reduced from 15 to
5 (`lfoRateTableHz` resized to match). `Application/Src/app.cpp`'s `audioSampleRateHz` corrected
from the nominal 32,000 Hz to the register-derived ≈32,552 Hz.

**Verification:** Host `host-debug` Catch2 suite (30/30) and STM32 `Debug`/`Release` presets all
build/pass clean. Real hardware: `Stm32I2sDuplexAudio::callCount()` reliably at/above the physical
~509/s ceiling with `errorCount` steady at 0, confirmed across multiple independent 1 s
measurement windows; PC-sampling confirmed the ISR is not saturating its budget. **Not yet
confirmed:** whether it actually sounds like the intended Deep Note — 5 voices is a thinner chord
than the reference's 15, and audible/musical correctness still needs a real listening test on the
user's end (line-out `J4` or headphone `J2`, pot channel 1 glides pitch from scattered to
converged). *(`voiceCount` subsequently pushed back up to 8 via real cycle-count profiling — see
the follow-up entry below.)*

---

## 2026-08-05 — THX Deep Note: DWT cycle-count profiling recovered 3 more voices

**Symptom:** Following the entry above, wanted to know whether 5 voices was a hard ceiling or
just where the (indirect, `callCount`-based) measurements happened to stop — `callCount` alone
can't distinguish "ISR finishes early" from "ISR barely keeps up" once it saturates the DMA's
fixed interrupt rate.

**Investigation:**

1. Added a proper, direct measurement: enabled the Cortex-M4's DWT cycle counter
   (`CoreDebug->DEMCR` `TRCENA`, `DWT->CTRL` `CYCCNTENA`) in `Stm32I2sDuplexAudio::start()`, and
   wrapped the `_processor.process()` call in `processHalf()` with `DWT->CYCCNT` reads to track
   the worst-case cycles one half-buffer's DSP work has taken. Exposed as
   `maxProcessCycles()`, printed alongside the existing `callCount`/`errorCount` diagnostic as a
   percentage of the real budget (`SystemCoreClock * FramesPerHalf / audioSampleRateHz`), using
   integer (tenths-of-a-percent) math to avoid depending on newlib-nano's optional float `printf`
   support.
2. Measured 5 voices directly: **75.0% of budget** (35421/47185 cycles) — confirms real,
   quantifiable headroom, not just a saturated/ambiguous `callCount` reading.
3. Re-tried 6 voices (previously measured indirectly as "~5% margin, borderline"): DWT gave
   **86.9%** (41014/47185) — tight but fits, `errorCount` still 0.
4. Suspected `-Os` was declining to inline the hot per-sample call chain
   (`ThxDeepNote::processBlock` → `ThxVoice::process` → `WavetableOscillator::process`/
   `interpolate`) across these header-only classes, given each call carries real Cortex-M4
   stack/argument overhead. Added `[[gnu::always_inline]]` to
   `WavetableOscillator::process()`/`interpolate()` and `ThxVoice::process()`, plus
   `[[gnu::flatten]]` on `ThxDeepNote::processBlock()` to force the whole chain to collapse into
   one function body. Re-measured at 6 voices: **72.1%** (34038/47185) — a ~17-point drop from
   the same voice count, confirming the hypothesis; flash size barely moved (+56 B), so this was
   genuinely removing call overhead, not just code that happened to already be inlined.
5. With the recovered budget, walked voice count back up, measuring after each step: 7 voices ->
   **81.7%** (38559/47185, ~4521 cycles/voice marginal cost, down from ~5593 pre-inlining) ->
   8 voices -> **92.1%** (43486/47185), `errorCount` still 0 at every step.

**Root cause (of the *recoverable* headroom, not the original overrun):** `-Os` was not inlining
across this call chain by default, so per-sample Cortex-M4 call/return overhead (stack
push/pop, argument marshalling) was a substantial fraction of the actual DSP cost — confirmed by
the ~17-percentage-point drop at a fixed voice count purely from forcing inlining, with no
algorithmic change at all.

**Fix:** `Galerna/Core/WavetableOscillator.hpp` (`process()`, `interpolate()`) and
`Galerna/Core/ThxVoice.hpp` (`process()`) marked `[[gnu::always_inline]]`;
`Galerna/Effects/ThxDeepNote.hpp`'s `processBlock()` marked `[[gnu::flatten]]`. `voiceCount`
raised 5 → 8. `Galerna/Platform/Stm32F405/Stm32I2sDuplexAudio.hpp` gained the permanent
`maxProcessCycles()` DWT diagnostic (kept, same rationale as `callCount`/`errorCount`:
authoritative answer instead of inference, useful for any future audio-path work on this board).

**Verification:** Host `host-debug` Catch2 suite (30/30) and STM32 `Debug`/`Release` presets all
build/pass clean at every voice-count step. Real hardware, direct DWT measurement, `errorCount`
steady at 0 throughout. **Known tradeoff, accepted deliberately:** 8 voices leaves only ~8%
budget margin (vs. 7 voices' ~18%) — a real, if currently-passing, risk against future
jitter/regressions eating that margin (a future code change touching the audio path should
re-check `maxProcessCycles()` against budget before assuming it still fits). Chosen anyway for
the denser chord. Audible/musical correctness is still an open item from the entry above — not
yet confirmed by ear.

---

## 2026-08-05 — THX Deep Note: audible "cracks" traced to hard int16 clipping, not interpolation

**Symptom:** User listened to the 8-voice build from the entry above and reported the audio
"sounds bad." Suspected the wavetable interpolation.

**Investigation:** Rather than guess between the two candidate explanations, added a direct
measurement: `DuplexAudioBlockProcessor::toInt16()` (`Galerna/Core/DuplexAudioBlockProcessor.hpp`)
changed from a `static` free function to a member method that increments a new `_clipCount`
whenever the pre-clamp value actually exceeds the int16 range, exposed via `clipCount()` and
printed alongside the existing `callCount`/`errorCount`/`maxProcessCycles` diagnostic. First real
measurement: **`clipCount=622`** over ~5 s (~148,000 samples processed, so intermittent — matches
"some cracks," not constant harsh distortion). Root-caused arithmetically: `ThxDeepNote`'s
`headroom = 3.0F / activeVoiceCount` scale, ported directly from the reference's
`3.0F / voicesToProcess`, was tuned for DaisySP's oscillator amplitude convention, not this
project's. With this project's per-voice raw peak (~1.55 wavetable Gibbs overshoot × 0.5 default
oscillator amplitude ≈ 0.775) and 6 voices, even 3 voices peaking together already exceeds the
+-1.0 float range the int16 conversion expects.

**Root cause:** `headroom` was too high for this project's specific oscillator amplitude scaling,
causing the summed signal to regularly exceed +-1.0 and hit the int16 conversion's hard clamp —
audible as intermittent crackle. Not an interpolation issue.

**Fix:** `Galerna/Effects/ThxDeepNote.hpp`'s `headroom` lowered `3.0F` -> `1.0F`.

**Verification:** Host suite and STM32 builds clean. Real hardware: `clipCount` dropped to `0`
across multiple consecutive readings (different pot positions), confirmed via both SWO prints and
direct GDB polling. `clipCount()` kept as a permanent diagnostic (same rationale as the others) —
this pattern (measure via a counter instead of guessing) caught a real bug that pure code review
would likely have missed, since the clipping only shows up under specific voice-phase alignment.

---

## 2026-08-06 — THX Deep Note: resonant filter added; voice count re-tuned around the new cost

**Symptom:** N/A — feature request ("could a filter be implemented, to add some character to the
sound?") plus a follow-up request to try raising voice count again now that the clipping bug
above was fixed.

**Investigation / implementation:**

1. Added `Galerna/Core/StateVariableFilter.hpp` — a Chamberlin state-variable resonant lowpass
   (`setCutoff(hz)`, `setResonance(0..1)`, `process(input)`), replacing `ThxDeepNote`'s one-pole.
   `timbre` (POT_4) now exponentially sweeps cutoff 150 Hz-5000 Hz; a new `resonance` control
   (wired to previously-unused **POT_8**) sweeps the resonant peak. Because a resonant peak is
   gain, and the headroom bug above was fresh, added output compensation (`1.0 - resonance*0.5`)
   proactively rather than waiting to find clipping again.
2. Stress-tested on real hardware with worst-case settings forced (converged pitch, brightest
   timbre, max resonance, full voice count) before trusting the normal pot-driven path: CPU
   **79.4%** of budget, `errorCount=0`. `clipCount` was nonzero (1190) but — checked via two GDB
   reads 3 s apart — frozen, not growing: a one-time startup transient (filter state starts at
   zero, briefly overshoots settling to the first real parameter values), not sustained crackle.
   This same "read clipCount twice, a few seconds apart" check became the standard way to
   distinguish a real bug from a harmless boot transient for the rest of this session.
3. Re-tried 8 voices (last tried in the entry two above, reverted to 6 after sounding bad — now
   suspected to have actually been the headroom bug, not the voice count itself). With the
   costlier resonant filter now in the signal path: **98.8%** of budget (46662/47185).
   `errorCount` stayed 0 and it technically kept up in that measurement, but with almost no margin
   for jitter — judged too risky to keep despite passing in the moment.
4. Tried 7 voices instead: **89.0%** (42037/47185), `errorCount=0`, `clipCount` transient-only
   (confirmed frozen across a 3 s window). Kept as final.

**Root cause:** N/A (feature addition), except for confirming the suspicion from the entry above —
the original "8 voices sounds bad" symptom really was the clipping bug, not a fundamental limit
on voice count; once fixed, the actual CPU-driven ceiling was higher, just lower than 8 once the
more expensive resonant filter was also added.

**Fix:** `Galerna/Core/StateVariableFilter.hpp` (new). `Galerna/Effects/ThxDeepNote.hpp`:
`setTimbre()` now maps to filter cutoff, new `setResonance()`, `voiceCount` settled at **7**
(`lfoRateTableHz` resized to match). `Application/Src/app.cpp`: `setResonance()` wired to POT_8.

**Verification:** Host suite (37/37) and STM32 `Debug`/`Release` clean at every step. Real
hardware at the final configuration (7 voices, resonant filter): 89.0% budget, `errorCount=0`,
`clipCount` confirmed non-growing.

---

## 2026-08-06 — LEDs repurposed to display the active THX voice count in binary

**Symptom:** N/A — feature request ("LEDs must show the number of voices used in binary" instead
of the old pot-driven blink demo).

**Implementation:** New `Galerna/App/BinaryLedDisplay.hpp` (`displayBinary(leds, value)`, LED0 =
least significant bit), 3 new host tests using the existing `FakeGpio`. `Application/Src/app.cpp`:
`GalernaApp::tick()` (the old blink driver, tied to POT_1/2/3) is no longer called each tick;
`updateThxControls()` now returns the active voice count it computed, and `App_Tick()` passes that
straight to `displayBinary(statusLed, ...)`. `GalernaApp`/buttons/switches/`ledPotMuxChannels`
wiring is left in place but unused, in case a future feature wants them — not deleted since the
request was scoped to LED behavior, not a broader cleanup.

**Verification:** Host suite (40/40). Real hardware: since this can't be judged by ear, verified
by reading `GPIOA->ODR` live via `openocd mdw` (non-intrusive, target running) and cross-checking
against the voice-count pot's SWO-printed value — `0x0C` (bits: LED0=0, LED1=1, LED2=1) = binary
110 = 6, exactly matching the voice-count pot reading at the time (max, i.e. 6 of 6 voices, on
the build flashed at that point).

**Current control layout** (`Application/Src/app.cpp`, physical pot labels from
`potMuxChannelName`): **POT_5** pitch, **POT_7** pitch shift, **POT_4** timbre (filter cutoff),
**POT_8** resonance, **POT_6** voice count (also drives the LED binary display). **POT_1/2/3**
unused since the LED change above. Audible/musical correctness of the full chain (7 voices,
resonant filter, binary LEDs) is still the user's to confirm by ear.
