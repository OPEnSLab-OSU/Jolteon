# Jolteon hardware issues and verification notes

Reviewed October 13, 2026 against the supplied **Jolteon_Nordic_Schematic.pdf**, the hardware-reference DOCX, **Atmel-42181-SAM-D21_Datasheet.pdf** pages 21-23 and the SPI DOPO table, and the linked [WT02E40E bridge firmware](https://github.com/joshbrew/WT02E40E-WiFi-BLE-UART-Bridge). The attached schematic takes precedence over older prose. These are schematic/source findings; they do not establish which components or modifications are present on the physical board.

## Findings to flag

| ID | Finding | Evidence and consequence | Suggested correction |
|---|---|---|---|
| SPI-01 | SPI0's labeled clock cannot be a SERCOM SPI clock | PB10 is SERCOM4 PAD2. SPI master SCK can use PAD1 or PAD3, not PAD2. Existing PB10 SCK / PB11 MISO / PB12 MOSI / PB13 radio CS cannot work as a hardware SPI pin set. This affects both RFM69HCW and microSD. | Swap the roles of PB10 and PB13: **PB13 SCK, PB11 MISO, PB12 MOSI, PB10 radio CS**; retain PB14 SD CS. SERCOM4 DOPO=0, DIPO=3. See the mux note below. |
| SPI-02 | SPI1's labeled clock also cannot be a SERCOM SPI clock | PB16 is SERCOM5 PAD0. Existing PB16 SCK / PB17 MISO / PB22 MOSI cannot work as a hardware SPI pin set. | Rotate the three signals to **PB17 SCK, PB22 MISO, PB16 MOSI**. SERCOM5 DOPO=0, DIPO=2. No additional MCU pins are needed. |
| UART-01 | WT02E40E host TX does not reach the firmware's configured RX | The PDF connects **PA22/TX1 to nRF P1.05 (E1)** and **PA23/RX1 to nRF P1.04 (E0)**. The linked bridge README/overlay configures nRF RX on **P1.06**, which the PDF connects to **PA15/IRQ_NRF (E2)**. This can produce module-to-host output with no usable host-to-module commands. | Prefer changing the bridge UART RX pin from P1.06 to **P1.05** in its overlay and rebuilding/flashing the module. Keep nRF TX P1.04. Alternatively change PCB routing so PA22 reaches P1.06, while relocating the IRQ net. Changing the SAMD SERCOM number alone does not fix this wiring mismatch. |
| PWR-01 | R20 is drawn as a 10-ohm shunt on VDD_NRF | In the nRF power block, R20 is connected from **VDD_NRF to GND**, and its value is **10**, not 10k. At 3.3 V this alone draws about **330 mA** and dissipates **1.09 W**, excluding the module. | Check the fitted resistor and board continuity before enabling the nRF rail. If R20 was intended to set output discharge, use the TPS22918's documented VOUT-to-QOD resistor arrangement; do not use a permanent 10-ohm rail-to-ground load. |
| SDI-01 | SDI-12 translator OE is tied to the wrong supply domain | U9 has VCCA=3VRAIL and VCCB=5VRAIL, but OE is tied to **5VRAIL**. TI references OE to VCCA and specifies its operating high level against VCCA. | Drive OE with a valid **3.3 V-domain** enable, with a pulldown and sequencing that keeps outputs disabled until both supplies are stable. Check the exact device ratings before rework. |
| SDI-02 | TXB0102 is not a suitable assumed open-drain field-bus interface | The document calls for open-drain operation, but TXB0102 is a push-pull translator. R25=10k also loads its A-side data line; TI normally recommends external pull resistors above 50k for TXB devices. A field cable adds load and capacitance beyond a short logic interconnect. | Redesign the SDI-12 interface around the standard's bidirectional bus, impedance, voltage, and slew-rate requirements. Do not treat changing firmware pin mode or substituting a generic I2C level shifter as proof of SDI-12 electrical compliance. |
| SDI-03 | SDI-12 idle polarity in the hardware reference is reversed | SDI-12 uses marking/logic 1 near **0 V**, spacing/logic 0 near **5 V**, 1200 baud, 7 data bits, even parity, one stop bit. The document's idle-HIGH description would invert the protocol. | Use the standard's inverted UART signalling: high-voltage break, then low-voltage marking before the command. Validate the field-side waveform with a scope. |
| MAP-01 | Early SERCOM assignments in the reference are incorrect | PA08/PA09 are **SERCOM0 C** or **SERCOM2 D**, not SERCOM3. PA22/PA23 are **SERCOM3 C** or **SERCOM5 D**, not SERCOM2. The later nRF example correctly uses SERCOM3. | Update the reference and firmware together. For the current UART nets use LTE **SERCOM1 C on PA16/17**, and nRF **SERCOM3 C on PA22/23**. Use SERCOM0 C for a future hardware-I2C implementation on PA08/09. |
| RADIO-01 | RFM69HCW is labeled LoRa, but is an FSK/OOK radio | The schematic and user-confirmed part are RFM69HCW. Its SPI protocol and packet tests are not those of an SX127x/SX126x LoRa transceiver. | Label it RFM69/FSK radio in firmware. Use RFM69 register checks and a matching FSK peer. A future LoRa replacement needs a separate driver and pin/IRQ review. |

### Confirmed startup clock wait from the October 14 J18 probe

The user-supplied SWD report verifies both installed images and reaches the
application reset handler at `0x3E7C`, then waits in Arduino `SystemInit` at
`0x401A`. `XOSC32K=0x060E` has the external crystal enabled while its ready bit
is zero (`PCLKSR=0x00005A18`). This confirms the startup wait; it does not identify
which physical component is at fault. A reset wire does not resolve this clock wait.

The supplied BOM specifies **X1 = ECS-.327-12.5-34G-TR**, 32.768 kHz, 12.5 pF
load, and C2/C3 = 22 pF (row 10 groups C50 with them incorrectly; see the BOM
table below). Check the fitted X1, solder joints, routing to PA00/PA01, and the
actual C2/C3/stray load against the crystal specification. Bootloader-to-sketch
handoff occurs before bootloader `board_init()`, so successful handoff alone
does not demonstrate that X1 oscillates.

Main-menu **L** can build an isolated crystal-less diagnostic configuration.
Use **1 → 2 (internal) → C → 0**, then main action **1 or 2** to compile/program
the matching sketch and loader when ready. The configuration must apply to the
application too; a bootloader-only replacement leaves its clock initialization
unchanged. Internal-mode USB clock recovery and timing without USB require bench
verification. The repository's documented external-crystal default remains intact.

### SPI mux and revision notes

Preserve the current net assignments as the default test placeholders. A pin-mapping check should report **FAIL** for hardware SPI0 and SPI1 on this revision. Do not configure another pad as an output and then claim that the labeled clock pin is working.

Software SPI can operate on arbitrary GPIO and may let the radio/card work on this board, but that result must be labeled **software SPI**; it does not resolve the hardware mapping failures.

For the proposed SPI0 set, **PB11 uses peripheral function D** for SERCOM4 PAD3; PB12/PB13 use function C. For the proposed SPI1 set, PB16/PB17 use function C and PB22 uses function D for SERCOM5 PAD2. Confirm these per-pin mux settings against the attached table when revising the board definition. MOSI and SCK must also match one of the SPI controller's DOPO combinations; selecting any three pins belonging to the same SERCOM is insufficient.

## Items requiring measurement or confirmation

| ID | Check | Why it matters | Verification |
|---|---|---|---|
| PWR-02 | LTE supply margin | The PDF powers LTE VCC from **3VRAIL**, through L4. SARA-R410M specifies a normal VCC range of **3.2-4.2 V**, nominal 3.8 V. A nominal 3.3 V rail has little margin for regulator tolerance and burst droop. The 5 V boost is not shown feeding LTE VCC. | Scope VCC directly at the modem during startup and transmission. Check regulator capacity, ferrite drop, decoupling, and minimum voltage. Do not feed 5 V directly to the modem. |
| I2C-01 | Host-side and selected-channel pullups | The PDF shows downstream 10k pullup networks, while the reference says some may be DNP. No dedicated upstream PA08/PA09 pullups are apparent in the reviewed blocks. Downstream pullups behind a deselected mux do not replace host pullups. | Check the fitted parts and measure idle high level and rise time on the upstream bus with all mux channels off, then on each populated branch. Do not use weak MCU pulls as proof that the designed bus is correct. |
| RTC-01 | RTC interrupt pullup | DS3231 INT/SQW is open drain. No dedicated PA19 pullup is apparent in the RTC block. | Fit a suitable 3.3 V pullup or explicitly enable an MCU pullup for the lab test. Verify a 1 Hz square wave separately from I2C calendar reads. |
| ADC-01 | Battery divider accuracy and source impedance | The PDF confirms **R11=348k** from VBAT to PB07 and **R12=750k** from PB07 to GND. The scale factor is **1.464** and source resistance about **238k**. At 4.2 V battery, PB07 should be about **2.869 V**. This is voltage sensing, not a current sensor. | Use long ADC acquisition, settling/discarded samples and averaging. Calibrate against measured VDDANA and a meter. Check the fitted values; do not confuse this divider with a 2:1 divider or a charge-status signal. |
| PWR-03 | Back-power paths and load-switch description | U10 is **TPS22918, a load switch**, not a separate voltage regulator. The reference reports back-powering. Driving host UART/reset pins while VDD_NRF is off can create another back-power path. | Keep module-facing signals high impedance while off, then enable/sequencing before UART. Measure off-state VDD_NRF and other rails with USB, battery, and module debug power combinations. |
| USB-01 | Existing USB-C revision notes versus this PDF | The DOCX reports a historical CC2=1k and a missing USB orientation connection, while this PDF labels both CC resistors 5.1k. The paper sources therefore disagree about the fitted revision. | Check the actual PCB: CC1/CC2 resistances, D+ mirrored pairs, D- mirrored pairs, ESD footprint and RESET pullup. Do not mark the physical USB issue fixed from the schematic labels alone. |
| BOOT-01 | Reported startup cause: bootloader overwritten incorrectly | User confirmed October 13 that the startup failure came from an incorrectly overwritten bootloader; flashing the official `samd21_sam_ba.bin` separately, followed by sketch-only at 0x2000, is the working recovery/workflow. The original supplied combined file itself had matched bootloader + padding + app. | Retain the standalone official-bootloader workflow. Program bootloader at 0x0000 and sketch-only at 0x2000, with deliberate BOOTPROT handling and verification. Treat the stock-pin compatibility findings separately from this corrected overwrite issue. Retest combined flashing only when needed to validate that workflow. |
| BOOT-02 | Stock Zero bootloader also has board-specific LED pin conflicts | Installed core 1.8.14 `bootloaders/zero/board_definitions_arduino_zero.h` defines LED PA17, RX LED PB03 and TX LED PA27. Its bootloader LED driver configures outputs when the monitor starts. On Jolteon these overlap LTE RX, a header/analog pin and radio RESET. The source checks for an application before entering monitor initialization, so this is especially relevant while remaining in bootloader mode; it does not establish the combined-image failure's cause. | For a future Jolteon bootloader, disable/remap all three LED definitions and review its PA10/PA11 SAM-BA UART usage against header loads. The sketch/core forced-include override cannot modify a precompiled bootloader. Retain the requested official binary in the current tools, but account for these drives during bootloader-mode tests, particularly a powered modem driving PA17. |

## BOM changes and additions

Workbook rows below are **Excel row numbers** in sheet `Jolteon_Dev_WT02E40E`. The BOM was reviewed, not edited. A routing correction can require no component substitution; a proposed design change still needs footprint, rating and layout validation before ordering.

| Priority | BOM row / references | Current entry | Suggested change |
|---|---|---|---|
| High | 47 / R20; 66 / U10 | R20 = **ERJ-3EKF10R0V**, 10 ohms; U10 = TPS22918TDBVRQ1 | The BOM confirms the low resistor value. Remove/DNP the permanent rail-to-ground load while reviewing the discharge design. If discharge is required, connect an appropriately sized resistor between VOUT and QOD as specified by TI; do not simply substitute 10k and assume the circuit is correct. |
| High | 43-44 / R11, R12; 56 / S2; new divider-enable parts | 348k / 750k permanently connected to VBAT; OS202011MS2QN1 switch | Integrate battery sensing with the **hardware power switch/state**. The reported approximately 0.5 V while off is a real observation; ADC protection-diode injection is a plausible cause to isolate. Switch the **top/feed of the divider**, or use a suitable powered-off-isolating analog switch/load switch, so PB07 is not driven when MCU power is absent. Keep R12 providing an off-state discharge path. A switched charger OUT/regulated rail is not automatically a raw-battery measurement; preserve access to actual VBAT when enabled. Do not gate only the bottom resistor, or rely on firmware/input mode to prevent off-state injection. |
| High | 12 / C27 | Primary **GRM188R60E476ME15D**, 47 uF, **2.5 V**; substitute GRM188R60G476ME01D is 4 V; schematic label is 100 uF | C27 is on the LTE supply. The primary part is underrated for a 3.3 V rail. Choose a higher-rated bulk capacitor with adequate effective capacitance at bias, ESR and burst-current performance; consider a larger footprint and at least 6.3 V rating as a design starting point. Reconcile 47 uF BOM versus 100 uF schematic. Do not approve the 4 V substitute for a redesigned 4.2 V-capable modem rail. [Murata specifications](https://ds.murata.com/simsurfing/mlcc.html?categ=spec&did=checkDetail&lcid=en-us&mdl=GRM188R60E476ME15D). |
| High | 63 / U9; 53 / R25 | TXB0102DCUR; 10k data pulldown | Replace/redesign the SDI-12 interface and correct OE to its VCCA domain. Treat it separately from U6's push-pull LTE UART use; do not replace both grouped U6/U9 blindly with one common part. Choose a field-bus topology with controlled drive/high impedance and validated cable/ESD behavior. |
| High | 38 / LTE_MOD; 34 / L4; 60 / U3; 12 and 16-19 / LTE capacitors | SARA-R410M-02B; BLM18EG221SN1D; TLV1117LV33DCYR; LTE currently on 3VRAIL | Review modem supply before substituting R5. Scope the present supply and design for the selected modem's voltage/current requirements. The requested R4-to-R5 migration is a **planned change**; verify lifecycle, exact R5 SKU, pin compatibility, peak power, antenna/SIM requirements and firmware changes rather than assuming a drop-in replacement. |
| High | 42 / R1, R2; 67 / USB; 58 / U1 | Both CC resistors already list **5.1k**; UJ32-C-V-G-TH-8-P24-TR; TPD4S012DRYR | Keep the intended 5.1k CC values, inspect the fitted CC2 reported as 1k, and correct assembly documentation if different. Join both D+ contacts and both D- contacts for both orientations; D+ to PA25, D- to PA24. Verify U1 symbol-to-package pin numbers and protection routing. Add D+/D- test pads. A BOM change alone cannot fix missing copper. |
| Medium | New MCU LEDs and resistors; existing 20, 25, 37, 46, 48 / CHG, PWR, LTE, LED0, LED1 and resistors | Existing LEDs primarily indicate power, modem or nRF behavior; no independent SAMD heartbeat/status LEDs are shown | Add **at least two, preferably three configurable MCU indicators**: heartbeat, status/activity and fault. Suggested GPIOs are **PA18, PA20, PA21**, retaining their header connections. Add individual series resistors; 1k-3.3k is a starting range to calculate from actual LED Vf and desired low-power current. Add an LED-enable jumper or DNP option. Keep rail-power, charge-status, MCU-state and radio-state indicators distinguishable. Avoid PA27/PB03 while using the unmodified Zero USB LED definitions. |
| Medium | 10 / C50, grouped with C2/C3; 6 / C48/C49 | C50 is grouped as a **22 pF crystal capacitor**, but the PDF places it on VDD_NRF; C48 is VIN bypass and C49 is CT | Split C50 out of the crystal row. Choose output bypass/bulk capacitance for the module and switch's inrush/rise-time needs; do not leave it at 22 pF because C2/C3 are crystal loads. Recalculate CT/rise time and verify available supply current. Keep actual crystal C2/C3 selection tied to X1 load and stray capacitance. |
| Medium | 53 / R26-R33 and R24; 65 / U8; new upstream I2C pulls | Downstream pullups grouped as 10k; TCA9548APWR | Retain mux RESET pullup R24. Add/verify independent **upstream** SDA/SCL pullups to 3.3 V. Mark each downstream pair DNP/populate explicitly by branch; select values from capacitance, speed and sink current. They are not redundant simply because an external sensor has pullups. |
| Medium | 64 / U7; new PA19 pullup | DS3231S#T&R; no dedicated INT/SQW pullup apparent | Add a suitable external pullup on PA19 to 3.3 V (for example 10k for the initial low-speed interrupt test, subject to loading). Preserve the optional test button to ground. MCU internal pullup is a lab workaround, not an assembly specification. |
| Medium | 59 / U2; 50-54 / charger-setting resistors; CHG indicator circuit | **BQ24070RHLR**, not BQ27xxx; STAT1/STAT2/PG are not MCU feedback in the reference | Route optional charger status outputs to spare MCU inputs/test pads with valid pullups, or document LED-only operation. Recheck CHG LED connection against the open-drain status truth table. Validate current settings, thermistor, timer and DPPM for the actual battery/source. Solar compatibility needs input-voltage and weak-source testing; this charger should not be described as an MPPT solution merely because it has a power path. |
| Medium | New RESET pullup/capacitor and test pads; MC1/RESET_M0 | External RESET conditioning is unclear in the reviewed revision | Add/verify approximately **10k RESET-to-3.3 V** and **100 nF RESET-to-GND** as the proposed baseline, then verify rise time and SWD/reset behavior against MCU guidance. This improves reset integrity; it is not proof that the combined-image startup fault is fixed. |
| Medium | 66 / U10; VNRF jumper; S2; D1/D2/D3/D7 and future protection parts | TPS22918 load switch; VNRF bridges 3VRAIL and VDD_NRF when closed | Document the jumper's intended fitted state. A closed VNRF bypasses switching, and the load switch should not be assumed to provide the required reverse-current isolation. Add an appropriate reverse-blocking power path where needed. Check battery cutoff, external-power coexistence and regulator reverse-current behavior as a system. |
| BOM cleanup | 3 / BAT1 | **XHP-2** listed as the PCB battery connector | XHP-2 is a cable **housing**, not the PCB header. Specify a PCB header matching the actual footprint/polarity, and list mating housing/contacts separately if supplied. [JST XH documentation](https://www.jst-mfg.com/product/pdf/eng/eXH-H.pdf). |
| BOM cleanup | 31 / L1, L7 | Description says 120 ohms / 5 A; **MPZ1608S300ATDH5** is 30 ohms at 100 MHz / 5 A | Reconcile the intended filter spec and purchased part. Update the text if 30 ohms is intended; otherwise select a validated alternative with acceptable current and DCR. Do not change impedance without rechecking the power path. [TDK specification](https://product.tdk.com/info/en/catalog/datasheets/beads_automotive_power_mpz1608_en.pdf). |
| BOM cleanup | 16 / C4, C21, C24, C25, C28, C41, C45 | Seven listed references, quantity **8** | Correct the quantity to seven or identify the missing eighth designator, then regenerate pricing. |
| BOM cleanup | 7 and 14 / C11 substitute, C32/C38 primary | Some part numbers are **CL03...** while the declared footprints are 0603 | Hold these entries for manufacturer package/dimension verification; remove any mechanically incompatible substitute. Do the same for C12's exact technology/land pattern. This is a purchasing/footprint check, not a claim that every listed capacitor is electrically wrong. |
| Documentation | 55 / RADIO; 35-36 / L5, L6; 2 / antenna connectors | RFM69HCW and a second frequency-band module are treated as substitutes; LoRa labels remain | Specify the exact installed RFM69 band and matching antenna/matching components. Different frequency variants are not unconditional substitutes. If changing to a true LoRa part, create a separate radio BOM/driver configuration. |

### Preserve these existing board observations

- **Switch off still leaves approximately 0.5 V:** investigate the always-live divider first, then module/debug/USB and regulator back-feed paths. Lift/isolate the divider feed during a controlled bench comparison to establish its contribution. Acceptance: no unintended rail powering in every intended off-state source combination, with leakage measured rather than inferred only from a high-impedance voltage reading.
- **nRF debug power powers the rest of the board:** check the VNRF bypass jumper and U10's reverse-current behavior, and keep module-facing host pins high impedance while off. Do not connect multiple regulator outputs together as a normal debug-power arrangement.
- **USB works in only one orientation:** verify the actual connector pairs and CC values on the board, even though the PDF/BOM show intended values. Test enumeration and serial transfers in both orientations after rework.
- **Bootloader/sketch startup:** user traced the reported failure to an incorrectly overwritten bootloader. Keep the working separate official-bootloader + sketch-only workflow and its correct 0x0000/0x2000 addresses. Supply/reset/back-power and stock bootloader LED-pin checks remain separate bring-up checks, not the diagnosed cause of this incident.
- **nRF controls awkwardly placed on J3:** regroup EN/RESET/IRQ and module debug connections near J4/J12 while preserving explicit pin labels and spare general-purpose header access. Update all three artifacts together: schematic, silkscreen/connector table, firmware pin map.
- **Battery cutoff and external-interface protection:** define battery undervoltage behavior, reverse polarity/current handling and ESD/overvoltage protection for USB, barrel input, antenna, SDI-12 and exposed GPIO. Select components from actual voltage, cable and environmental requirements; a universal TVS or diode substitution is not established by this review.
- **Via close to battery VDD:** increase clearance as practical and explicitly specify solder-mask coverage/tenting in fabrication files. Review the manufactured mask and copper clearance rather than relying on the schematic.

### Arduino Zero core conflict

The installed Arduino SAMD core 1.8.14 maps **USB TX activity LED to PA27** (Jolteon radio RESET) and **USB RX activity LED to PB03** (Jolteon analog/header pin). `USB/USBCore.cpp` configures and toggles them. This can hold/reset the RFM69 or drive an external analog signal while the USB console is active.

Disable those USB LED macros in a Jolteon-specific build/variant. Merely undefining them in the sketch does not change separately compiled core files. The verification package uses a core-wide C++ forced-include override; a proper J18 board variant is the lasting fix. Do not add indicators to those nets as a workaround for the pin conflict.

## What firmware can establish

The current verification sketch retains both invalid SPI pin maps as explicit
failures and provides `spi soft` only as an opt-in workaround. Its forced-include
build override disables the conflicting Zero USB LED pins in the core as well
as the sketch. The local WT bridge update now provides a Jolteon-specific
RX=P1.05 overlay and independent `tx both` forwarding; applying those firmware
changes to the physical module remains a separate flashing step. Automatic
nRF power in the sketch is gated by `NRF_POWER_PATH_VERIFIED` pending the R20/U10
check. Software changes do not establish that any PCB or power issue is fixed.

The expanded sketch now centralizes pinouts/options in `config.h`, rejects
overlapping assignments, runs a client-independent baseline, and displays local
heartbeat/TX/RX/test-result patterns on PA05. Optional separate TX/RX LEDs use
PA20/PA21 on the current map and require physical LEDs/resistors; they remain
disabled until wired. PA13 drives EN_NRF high during an explicit power-up, with
reset held low through a configurable settling delay; UART starts afterward.
No connected BLE/Wi-Fi client is required for the local indicators.

The `boot` command checks only the configured bootloader image bytes by CRC32,
excluding unused padding; its default reference is the official 6504-byte Zero
binary in Arduino core 1.8.14. Configure/disable that comparison when using a
custom bootloader. BLE and Wi-Fi now have same-transport token roundtrip tests,
and report retries retain immutable tag/content. The optional revised SDI-12
profile includes receive/parity/framing/address/deadline checks; the original
TXB0102/OE circuit remains blocked and electrical compliance still needs bench
tests. Charger/solar/off-state power behavior cannot receive an automated PASS
with the current unconnected status nets.

- Hardware SPI pin-map checks can conclusively reject the current unsupported clock pads.
- UART responses establish communication only after the module firmware and PCB RX wiring match.
- TCA9548A register readback establishes mux control; an empty downstream scan does not prove connector continuity.
- RFM69 register read/write establishes SPI communication. RF TX/RX and IRQ require a matching peer and an antenna appropriate to the installed band.
- SD mount/read and a newly created file's write/readback establish filesystem communication, without overwriting an existing file.
- Battery ADC readings require divider/supply calibration; they cannot verify charge current or unused charger status nets.
- Rail voltage, signal integrity, SDI-12 electrical compliance, and off-state back-powering still require measurement.

## Apply the separate PCB revisions deliberately

Additional sources: **Jolteon Hardware Bring-up.pdf** (nine pages, revision history July 27, 2026) and **Jolteon_Rewire.pdf** (single sheet exported October 13, 2026). They describe a separate Feather/Adafruit-based carrier. Its reference designators and Arduino GPIO numbers are **not** the Nordic PCB's reference designators or SAMD PA/PB pin map. They provide design changes to port, not proof that the original board already includes those changes.

| Revision to carry over | Proposed change to Nordic board / BOM | Checks before adopting |
|---|---|---|
| Dedicated LTE 3.8 V supply | Add a supply based on the rewire's **AP7361C-Y5-13 adjustable LDO**, fed from the 5 V boost, or another regulator meeting the selected modem's load requirements. The shown feedback is **7.5k top / 2k bottom** with 0.8 V reference: 0.8 x (1 + 7.5/2) = **3.8 V**. Add the regulator, feedback resistors, input/output capacitors and suitable bulk capacitance to the Nordic BOM; reconnect LTE VCC/L4 to that output. | Use the adjustable variant, not a fixed **AP7361C-33Y5-13**. This is an **LDO**, not a switching buck: 5-to-3.8 V dissipation is 1.2 x load current watts. Validate temperature, transient droop, boost headroom and capacitor derating. Do not declare the design sufficient from the DC feedback equation alone. [Diodes datasheet](https://www.diodes.com/assets/Datasheets/AP7361C.pdf). |
| Correct modem voltage rationale | Treat **3.8 V as the intended modem rail**, separate from 3.3 V logic and 5 V sensors. | At 3.3 V, SARA-R410 has little margin above its 3.2 V normal lower limit before burst droop; **5 V is overvoltage**, not another brownout threshold. Confirm the selected R5 variant's limits independently. |
| Direction-controlled SDI-12 translation | Replace Nordic **U9 TXB0102** with a redesigned **SN74LVC1T45DCKR** circuit modeled on rewire **U7**. Keep MCU data on a documented GPIO; add a **separate DIR control GPIO**. VCCA=3.3 V, VCCB=5 V; DIR low receives B-to-A, DIR high transmits A-to-B. It is a different package and pinout, not a drop-in replacement. | The 1T45 has **DIR**, not an independently controlled OE pin. Firmware must release MCU data before receiving and coordinate direction to avoid contention. Validate the shown series resistance, RC shaping, pulldown and protection against the SDI-12 standard and actual cable/sensor loads. Do not copy the old TXB OE treatment into the new circuit. [TI datasheet](https://www.ti.com/lit/ds/symlink/sn74lvc1t45.pdf). |
| Independently switched 12 V sensor supply | Retain or redesign Nordic U5/TLV61046A stage, but add a defined **12V_EN** controlled by the MCU, with a default-off pull resistor. The data DIR and sensor power enable must be independent. Add the associated control routing/components. | The Feather diagram's **GPIO13** is not permission to use SAMD PA13: PA13 already enables nRF. A candidate Nordic allocation is **PA12 for SDI DIR**, **PA18 for 12V_EN**; review header assignments and update firmware/silkscreen. Reassign suggested LEDs around those controls, for example PA20/PA21 and another reviewed spare pin. |
| SD-card deselect and decoupling | Add **10k SD CS-to-3.3 V pullup** on Nordic PB14, plus the reviewed local card bulk/bypass capacitance (the rewire shows 10 uF plus 100 nF). Keep radio CS high when the card is active. | A CS pullup fixes reset/boot deselection, not SPI0's unsupported clock pad. Fix the MCU routing separately. |
| RTC interrupt pullup | Carry over the rewire's **10k INT/SQW-to-3.3 V** pullup to Nordic PA19. | Confirm INT/SQW output operation and connector/button loading. |
| Charger/source architecture | Consider the revised **BQ24074** power path, TS-jumper/thermistor arrangement, input isolation and charger-status wiring as a separate design update. The Nordic BOM currently specifies **BQ24070**, while the rewire symbol contains a conflicting **bq25895** label. | Select one actual charger and matching symbol/pinout/BOM. The bring-up text itself gives conflicting charge-current statements for RISET=1.6k (approximately 890 mA versus 556 mA); recompute ISET/ILIM/ITERM from the **chosen** IC's datasheet. Do not transfer the existing resistor values across charger types without checking. Verify weak-source/solar behavior and temperature. [BQ24074 datasheet](https://www.ti.com/lit/ds/symlink/bq24074.pdf). |
| Boost power paths and layout | Feed high-load boosts from the reviewed system power-path node, not through the Feather's small logic regulator; port the same principle to the custom board. Use short high-current return paths and preserve ground continuity. | Follow each regulator's own placement/current-loop guidance. The bring-up notes' blanket advice about copper under inductors is not a substitute for keeping switching nodes and noisy loops away from sensitive nets. Size supply and ground copper for the actual load. |
| R5/GNSS and SIM revision | The rewire uses **SARA-R510M8S-01B-01**, adds GNSS antenna/protection and references SIM protection to **VSIM**. Port those changes only if that specific modem is selected for the new Nordic revision. Keep the current verification sketch's default R410 profile. | Update module footprint, reserved pins, GNSS antenna path, power and AT support together. Do not assume the SIM supply is always 1.8 V for the revised module. Keep optional antenna detection/matching parts explicitly DNP or populated according to the antenna circuit. |
| I2C voltage selection | Keep upstream MCU I2C and mux logic supply at **3.3 V**, and document optional downstream voltage/pullup selectors. | Ensure no selector ties a 5 V pullup directly onto a non-5-V-tolerant MCU pin. Mux address straps/pull resistors must be defined; do not import alternate GPIO-controlled address bits into the fixed-0x70 Nordic configuration accidentally. |
| Via tenting and labeling | Carry over clearer power/connector labels, extra test points, shorter traces and explicit CAM via-tenting requirements. | Inspect final fabrication outputs. Tenting, RF impedance/return paths, clearances and antenna placement require board/layout review; the schematic alone does not establish them. |

The separate carrier's **A0 x 2 voltage measurement** is not the Nordic board's battery scale: its existing R11/R12 values give **x1.464**, until the battery-sense circuit is deliberately changed. The switch-controlled battery-divider correction remains required even if the alternative charger/3.8 V architecture is adopted.

## Option: keep stock Arduino Zero bootloader pin compatibility

The stock Zero source reserves **PA17** as its main LED, **PB03** as RX LED and
**PA27** as TX LED. Its default build also enables SAM-BA UART on **PA10/TX and
PA11/RX**, alongside USB. These pins may still be broken out, but externally
connected circuits must tolerate their bootloader functions. Prefer reserving
the three LED pins for the additional indicators already requested, rather than
connecting them to independently driven peripheral signals.

One small hardware remap that avoids the currently identified conflicts is:

| Signal | Current | Stock-compatible candidate | Firmware change |
|---|---|---|---|
| LTE RX at the SAMD | PA17 | **PA18**, mux C, SERCOM1 PAD2 | Keep TX on PA16/PAD0; change UART RXPO from 1 to 2. |
| Radio RESET | PA27 | **PA20** as a GPIO | Update radio reset pin configuration. |
| Analog/header assignment | PB03 | Another reviewed free ADC-capable pin, e.g. **PB04/AIN12** | Update the connector/ADC map; do not tie PB03 and PB04 together. |
| Indicators | None on these MCU nets | **PA17, PB03, PA27**, with suitable series resistors | Match the stock bootloader's LED polarities: main PA17 active high, RX/TX active low. |

This is an optional **future revision**, not the current test sketch's pin map.
PA18 had also been suggested for future 12 V enable and PA20 for an added LED;
choosing this stock-compatible layout requires reallocating those proposed uses
(for example review PA21 for 12 V enable) and reviewing all header assignments.
Keep PA10/PA11 clear of loads that conflict with bootloader UART operation. USB
remains PA24/DM + PA25/DP; reset, clock and SWD circuitry must still meet the MCU
and bootloader requirements.

A **Jolteon-specific build of the existing Arduino SAMD bootloader**, plus a
Jolteon Arduino variant, is the more flexible alternative. Disable/remap its LED
and optional UART functions, set safe board-control defaults, retain native USB
and the current 8 KiB application offset initially, and verify double-reset
entry/application handoff. This means adapting the proven source; it does not
require writing the bootloading protocol from scratch. It also does not fix the
invalid SPI clock routing, USB-C copper, power-path or SDI-12 circuit issues.

References: [Arduino Zero bootloader definitions](https://github.com/arduino/ArduinoCore-samd/blob/master/bootloaders/zero/board_definitions_arduino_zero.h),
[LED driver](https://github.com/arduino/ArduinoCore-samd/blob/master/bootloaders/zero/board_driver_led.h),
[default bootloader interface build settings](https://github.com/arduino/ArduinoCore-samd/blob/master/bootloaders/zero/Makefile),
and the supplied SAMD21 mux table for PA18/SERCOM1 PAD2.

## Sources

- Supplied `Jolteon_Nordic_Schematic.pdf`, single sheet dated August 10, 2025: net connections, component values and installed models.
- Supplied `Atmel-42181-SAM-D21_Datasheet.pdf`, pages 21-23: pin multiplexer table; SPI CTRLA.DOPO section: permitted MOSI/SCK pad pairs.
- Supplied OPEnS Jolteon hardware reference: connector numbering and reported prototype/revision notes.
- [WT02E40E bridge README and UART overlay](https://github.com/joshbrew/WT02E40E-WiFi-BLE-UART-Bridge): UART shell prefix `wt`, 115200 8N1, nRF TX P1.04 / RX P1.06.
- [TPS22918 datasheet](https://www.ti.com/lit/ds/symlink/tps22918.pdf): load switch and QOD connection.
- [TXB0102 datasheet](https://www.ti.com/lit/ds/symlink/txb0102.pdf): OE domain, push-pull operation, external pull-resistor guidance.
- [SARA-R4 datasheet](https://content.u-blox.com/sites/default/files/SARA-R4_DataSheet_UBX-16024152.pdf): VCC limits and modem control signals.
- [RFM69HCW datasheet](https://cdn.sparkfun.com/datasheets/Wireless/General/RFM69HCW-V1.1.pdf): FSK/OOK modulation and register interface.
- [DS3231 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf): open-drain interrupt and register interface.
- Local Jolteon `datasheets/SDI-12_version-1_4-Jan-10-2019.pdf`, sections 3.1 and 4: bus logic levels, frame format and break timing.
