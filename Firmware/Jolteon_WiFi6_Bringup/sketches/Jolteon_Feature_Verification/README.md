# Jolteon SAMD21J18 feature verification

This sketch extends the external PA05 heartbeat with an interactive bring-up
console for the current **SARA-R410 + RFM69HCW** board. Pin names are physical
SAMD ports, taken from the supplied Nordic schematic. They are not Arduino Zero
digital pin numbers. The original sketch is available under `../SAMD21J18_Blinky/`
and remains unchanged as the parent folder's `SAMD21J18_Blinky.ino`.

## Build and flash

From the repository root, run `python tools/build_samd.py verification`.
This selects the Jolteon profile/custom loader, supplies the matching checksum,
and checks OpenOCD files without flashing. Add `--flash` when ready to program.
Use `--adapter gpio` on Pi 4 or the default ST-LINK adapter elsewhere.

For Zero plus the official loader, use
`python tools/build_samd.py verification --board zero --bootloader official`.
Zero plus the custom loader is also supported. Both paths disable conflicting
core USB LED outputs. IDE Verify works with the installed Jolteon profile;
Zero IDE builds need the forced override supplied by the helper.

See `../../README.md` for setup. Scratch/logs go under `../../.build`; checked
images and source manifests go under `../../releases/samd`.

Self-contained compatibility launchers are centralized under `../../tools`.
From the repo root, `python tools/copy_tools.py windows sketches/Jolteon_Feature_Verification`
copies the Windows folder here; use `linux` or `mac` for the other platforms.
Copied tools find the .ino in their own folder or parent. They compile Zero and
default to the pinned official bootloader; use the repo helper to automatically
match a custom loader's checksum.

The stock Zero loader uses PA17, PA27 and PB03 as LEDs in its monitor. The custom
Jolteon loader removes those assignments. See BOOT-02 in HARDWARE_ISSUES.md.
No physical target testing was performed during this copy.

## First test

Open the native USB serial port at 115200 and send newline-terminated commands:

```text
help
pins
all
summary
```

The baseline runs once after five seconds, even without USB, Wi-Fi or BLE clients.
Set `RUN_BASELINE_ON_BOOT=false` in `config.h` to make it manual. `all` starts a
fresh run and clears older outcomes; `summary` shows results, timestamps and
counts. It leaves nRF power unchanged, sends no RF packets, writes no SD files,
and does not configure modem network profiles. RTC SQW control is temporarily
changed and restored. Peer/jumper/measurement tests get explicit BLOCKED reasons.

| Feature | Commands and expected interpretation |
|---|---|
| Device / boot | `info`: device ID, flash size, reset causes, clock, app vectors, BOOTPROT and USB state. `boot`: read-only CRC of the selected bootloader image, excluding padding. The repository helper supplies the official/custom image's identity automatically. |
| Current SPI0 / SPI1 maps | `spi map`: **FAIL is expected for both** because their labeled clock pads cannot be hardware SCK. |
| Software SPI workaround | `spi soft`, then radio/SD tests; this does not turn either hardware-map failure into a pass. `spi hard` restores the default. |
| General SPI loopback | Jumper the selected bus's MOSI to MISO, then `spi loop 0` or `spi loop 1`. Chip selects stay high. Remove the jumper afterward. |
| nRF power/reset/UART | Inspect R20/U10, confirm module RX=P1.05 firmware, then `nrf on`, `nrf probe`, `nrf reset`, `nrf off`. OFF releases module-facing UART/reset/IRQ pins. |
| Wi-Fi / BLE | `wifi scan`, `wifi status`, `ble test`, `ble tx`, `wifi tx <ip> <port>`. The last two start a unique-token peer challenge; return `tx uart "j18 peer <received-token>"` through the same BLE or Wi-Fi transport within 60 s. USB/local queue acknowledgements cannot pass the roundtrip test. A timeout is FAIL. |
| Bridge commands | `nrf cmd ble status json` adds the `wt ` UART prefix. `nrfraw <line>` sends an exact line. `bridge nrf` opens a USB-only raw terminal; `~.` exits. Raw bridging is rejected from wireless commands. |
| SARA-R410 UART / AT | `lte probe` checks AT termination, expected model, SIM READY and registration. If needed, `lte on` sends a PWR_ON gate pulse. `lte AT+CSQ`, `lte AT+CEREG?`, USB `bridge lte`. Hard reset is explicit: `lte reset`. IP data sessions remain BLOCKED pending APN and an external endpoint. |
| I2C and TCA mux | `i2c scan`, `mux test`, `mux 0` through `mux 7`, `mux off`, `i2c recover`. Mux test checks reset/register readback and scans exposed channels 0â€“3, then restores the previous mask. Root devices also appear on branch scans. |
| General I2C transfers | `i2c read <addr> <reg> <count>` and `i2c write <addr> <reg> <byte>`; addresses/numbers accept decimal or `0x` notation. Writes act on the addressed device. |
| RTC / IRQ | `rtc` validates BCD, date/leap-year/hour fields, advancing seconds and OSF; `rtc irq` temporarily enables 1 Hz SQW, counts physical transitions and restores control. Its internal pullup is a bench workaround and is released afterward. |
| RFM69 registers | `radio test` checks version 0x24, scratch-register write/read/restore and ModeReady. RFM69 is FSK/OOK, not LoRa. |
| RFM69 over-air / IRQ | `radio tx <Hz> <text>` or `radio rx <Hz>` with the correct antenna/module band and matching FSK peer. Profile: 50 kbps, about 25 kHz deviation, sync 2D D4, variable packet length, CRC. RX waits up to 10 s. TX requires peer confirmation. |
| SD | `sd read`, `sd list` (at most 32 root entries / 5 s). `sd test` creates a new uniquely named file, writes/checks 1024 bytes, closes/reopens/verifies it and removes that file. Existing names are never overwritten. No formatting operation is provided. |
| Battery | `battery`: raw PB07/AIN15, ADC voltage and scaled battery voltage. Divider is 348k/750k, **x1.464**, not x2. Calibrate VDDANA and compare to a meter. No battery-current or charger certification is inferred. |
| GPIO / ADC | `gpio read PA02`, `gpio write PA05 1`, `gpio release PA05`, `adc PB07`; use `blink off` before repurposing the heartbeat. Reserved peripheral pins cannot be general outputs. |
| GPIO loopback | Jumper two free header pins, e.g. PA02 to PA03, then `gpio loop PA02 PA03`. A 32-bit pattern is checked with weak input pulls opposing the output, preventing an unconnected/constant input from passing. Pins are released; an enabled heartbeat resumes. |
| IRQ observation | `irq nrf`, `irq radio`, `irq rtc` count transitions for 3 s. Observation alone does not establish a peripheral event. |
| SDI-12 | `sdi 0I!` is **BLOCKED** on the original TXB0102/OE circuit. The optional revised DIR-controlled profile supports TX/RX, inverted 1200 7E1, parity/stop checks, CRLF/address validation and bounded deadlines. It requires physical rework and sensor/scope testing. |
| Charger / solar / off-state leakage | Explicitly BLOCKED as an automated pass: PGOOD/STAT1/STAT2 have no MCU inputs. Follow the measurement checklist in `HARDWARE_ISSUES.md`; PB07 is battery voltage, not current. |

`PASS` means the stated check passed; `INFO`, `BLOCKED` and `NOT RUN` do not imply
full feature verification. External peers, test jumpers and measurements remain
necessary for complete board validation.

## Visual confirmation without a client

All indicators run locally on the SAMD21 and need no USB or wireless connection.
PA05 is the existing external blinky/heartbeat pin. It also displays:

| Pattern on PA05 | Meaning |
|---|---|
| Slow 500 ms on/off | Running and idle |
| Fast 100 ms on/off | Baseline running, when no higher-priority event is displayed |
| Two short pulses | UART transmit attempt |
| Brief single flash | UART bytes received/processed |
| Two longer pulses | A check passed |
| Four short pulses, repeated every 2 s | A check failed; latched until `led clear`/`clear` or the next baseline |

Traffic patterns take precedence, so a sustained UART stream can mask other
patterns briefly. `led test` demonstrates TX/RX/PASS/FAIL patterns and clears the
visual fault latch; it does not change recorded test results. `led status` shows
enabled settings. `blink off` stops automatic PA05 writes for GPIO reuse.
Expected failures on the original SPI maps also set the visual fault latch.

For separate TX and RX indicators, wire **PA20 -> series resistor -> LED -> GND**
and **PA21 -> series resistor -> LED -> GND** (for example 1k series resistors),
then set `J18_ENABLE_EXTRA_LEDS` to 1 in `config.h`. Their default active level is
HIGH. Extra indicators are disabled by default because the current PCB has no
fitted SAMD indicator LEDs and these header pins may have another attached load.
The optional stock-compatible profile moves TX LED to PB04 because PA20 becomes
radio RESET. Confirm the connector assignment before wiring.

A TX pattern proves a local transmit attempt. A power-control readback proves the
GPIO state. Neither proves rail voltage, radio transmission or remote delivery.

## Reports when USB is unavailable

USB is the primary console. Without an open USB console, output is queued for
the WT02E40E; BLE is the default wireless destination. The bridge update in
`../../bridge` provides independent `tx both` routing and the Jolteon UART
RX=P1.05 overlay. Flash that build to the module separately.

Before automatic module power can run, correct/confirm the R20/U10 power circuit
and UART wiring, set `J18_NRF_POWER_PATH_VERIFIED` to 1 in `config.h`, and rebuild.
After five seconds without USB, the sketch enables the module and BLE. Until
that flag is true, automatic nRF power is blocked; the explicit USB `nrf on`
command remains available for a reviewed bench setup.

Power-up drives **PA13 / EN_NRF high** while holding PA14 reset low, waits the
configured supply-settling interval, releases reset to high impedance, then
starts PA22/PA23 UART and waits for module boot. Power-off lowers PA13 and releases
UART/reset/IRQ signals. The delays are configurable; verify the actual CT ramp
and VDD_NRF with instruments rather than inferring them from a delay or LED.

Connect a BLE client and subscribe to TX notifications. Send commands to the
bridge's BLE command characteristic:

```text
tx uart "j18 all"
tx uart "j18 summary"
tx uart "j18 report status"
```

Subscribe to command-response notifications too to see bridge acknowledgements.
The bridge web console reassembles streamed messages when needed for the
negotiated BLE MTU. Reports have a `J18#<sequence>` prefix; ` | ` represents a
line break inside a report chunk.

```text
report auto
report both
report wifi 192.168.1.50 5000
report wifi 0.0.0.0 0
report status
report off
```

`auto` sends to USB while open, otherwise wireless; `both` also sends wireless
while USB is open. A Wi-Fi target adds UDP output alongside connected/subscribed
BLE. Configure Wi-Fi credentials/association separately with the bridge commands
and run a UDP receiver at that destination. Wi-Fi UDP has no connection flag
identifying a listening client.

Reports are retained in a bounded 6144-byte ring until the bridge accepts a
chunk. Retries preserve both the sequence tag and payload; a delayed reply cannot
acknowledge a different chunk. `report status`/`summary` expose queue overflow,
acknowledgements, timeouts, remote-command drops and receive faults. Bridge
acceptance proves local queueing, not remote delivery. Reports generated before
USB opens are replayed to USB when it connects.

Diagnostic nRF transactions also feed the command/ack parser. There is one pending
remote-command slot; send commands sequentially and inspect `remote_drop` if a
client sends several before the current test completes. Reporting pauses during
exclusive UART/SDI operations, and UART ring overflow remains possible under a
continuous stream. This is a bring-up console, not a lossless logger.

## Hardware revision settings and included library

`config.h` contains the physical pinout, profile selectors, baud rates, power
delays, indicators, queue limits, battery-divider calibration and bootloader CRC.
Changing a pin in this file does not change PCB copper. Hardware UART currently
supports nRF SERCOM3 PA22/PA23 and LTE SERCOM1 PA16/PA17 (or PA18 in the optional
profile); unsupported UART routes are rejected. Hardware SPI validates actual
pins against its supported SERCOM routes; arbitrary flag changes cannot make an
invalid map pass.

`J18_REVISED_SPI0` and `J18_REVISED_SPI1` default to 0, preserving the actual PCB
maps. Set them to 1 only after making the corresponding suggested rewiring:

```text
SPI0: PB13 SCK, PB11 MISO, PB12 MOSI, PB10 radio CS, PB14 SD CS
SPI1: PB17 SCK, PB22 MISO, PB16 MOSI
```

Set `J18_REVISED_SDI12=1` only for the rebuilt 1T45 circuit: PB15 data, PA12 DIR,
PA18 independent 12 V enable. `sdi power on`/`off` controls sensor power, then
`sdi 0I!` performs an identification exchange. This driver keeps interrupts
enabled and rejects gross sampling slips; scope/cable testing is still required.
Its DIR low / data-input state is restored on success or failure.

`J18_STOCK_BOOTLOADER_PINMAP=1` selects future LTE RX PA18 (RXPO=2) and radio RESET
PA20. This conflicts with the proposed SDI PA18 power enable; compilation rejects
that combination until you choose another power-enable pin in `config.h`.

`HARDWARE_ISSUES.md` consolidates findings, the existing revision list, BOM row/
designator references and changes from the separate Feather carrier, including
SDI-12 DIR + separate 12 V enable and a reviewed 3.8 V modem supply. Those future
circuits are not silently selected by this sketch.

SdFat 2.3.1 is bundled under `src/SdFat` from upstream commit `cda0573`.
Its MIT notice is in `SdFat_LICENSE.txt`. Changes: custom SPI driver selected,
custom chip-select callbacks selected, SD CRC enabled, and five internal includes
made relative so Arduino compiles the library inside the sketch's `src` folder.
No extra library installation is needed.

Compiled with Arduino SAMD core 1.8.14 / Zero Native USB on October 13, 2026.
Compilation/configuration checks do not establish behavior on the physical board.

`tests/run_tests.bat` runs host tests for the actual protocol, report queue and
indicator code using simulated serial I/O and time. Add `/DJ18_ENABLE_EXTRA_LEDS=1`
to exercise extra indicators. The same tests can be built with a C++14 compiler
using `tests/host` and the sketch root as include directories, compiling
`tests/test_logic.cpp`, `Protocol.cpp`, `Telemetry.cpp` and `Indicators.cpp`.
The separate SDI test compiles the actual driver and simulates timed TX/RX levels,
including parity/framing/address errors, buffer bounds, timeouts, DIR release and
independent sensor power. It does not establish physical voltage/slew/cable or
interrupt timing behavior. Tests also reject overlapping optional pin profiles
at compile time. `tests/check_profiles.ps1` compiles the optional Arduino profiles
without changing the defaults or accessing hardware. SAMD register operations
and the physical board still require bench verification.

## Optional Jolteon bootloader and board profile

`../../bootloader` contains a source-built Jolteon SAM-BA loader, an optional
Arduino board profile, and a repeatable procedure for future pin revisions.
The loader keeps the application at `0x2000` and avoids the Zero boot LED
assignments on LTE RX / radio reset / PB03. Its PA05 indicator and early safe
module power/CS states are independent of USB, BLE or Wi-Fi clients.

With **OPEnS Jolteon SAMD21J18 (Native USB)** selected (`opens:samd:jolteon_j18`),
this sketch compiles directly in IDE/CLI: the variant disables conflicting USB
LEDs and supplies its generated custom bootloader byte count/CRC. Its UART
interrupt handlers override the variant's weak handlers. Existing SPI routing
failures and nRF automatic-power guard remain explicit defaults.

Compatibility launchers and `../../releases/baseline/verification-zero` use **Arduino Zero**
and the **official Arduino bootloader**. To flash the custom loader with those
launchers, use the separate-bootloader variant and put the intended
`samd21_sam_ba.bin` beside this sketch. Also provide the custom identity macros
from `../../bootloader/firmware/JolteonBootloaderIdentity.h` when compiling,
or update both expected boot constants. Keep the existing forced
`J18BuildOverrides.h` for Zero builds. A custom bootloader does not change the
Zero core's pin aliases or USB LED settings.
