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
