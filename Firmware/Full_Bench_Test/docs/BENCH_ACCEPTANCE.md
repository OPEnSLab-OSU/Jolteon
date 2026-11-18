# Jolteon Rewire physical bench acceptance

Status: **NOT RUN**. Build/host checks do not fill this sheet automatically.
Copy it to a dated bench record when testing; keep Serial logs and scope captures
beside it. Do not store useful physical-test evidence only in `.build/`.

- Board revision / modifications / serial: ______
- Schematic SHA256 (`WIRING_SOURCE.json`): ______
- Firmware SHA256 (`firmware/BUILD_MANIFEST.json`): ______
- Operator / local date-time / environment: ______
- Power source / current limit / loads / battery state: ______
- Meter / scope / ADC reference voltage: ______

## Optional external power measurements

Confirm polarity, unpowered resistance and carrier/Feather supply connections.
This sheet records optional physical measurements; it is not a firmware gate.
Use `rails off`, `rails 3`, `rails 5`, `rails on` and `rails 12` to select the
states below. `rails on` / `hypnos enable` call the real Hypnos enable API;
`rails off` / `hypnos disable` call disable. Protocol commands automatically enable 3V/5V; SDI data also enables sensor 12V. No voltage
entries are required. Use `rails 12 on/off` for
the independent sensor boost, and capture the printed API name and GPIO levels.

| Stage | +3V3 | 5VOUT | 3VRAIL | 5VRAIL | VCC_3.8V TP20 | 12VRAIL | Current | Result |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OFF | | | | | | | | |
| 3V ONLY | | | | | | | | |
| 5V ONLY | | | | | | | | |
| BOTH | | | | | | | | |
| 12V ON | | | | | | | | |
| BOTH / 12V OFF again | | | | | | | | |

Record BQ_IN/BQ_OUT/VBAT_FEATHER and battery/charge measurements separately.
U3 is always-on; D5 switches its 3VRAIL output path only. D6 controls two
switches: 5VRAIL and HIGHVRAIL. SARA VCC and gas 5V feeds use unswitched 5VOUT.
For HIGHVRAIL, select a reviewed input/load and measure OFF leakage, ON voltage,
drop and current: ______. The console does not measure this external rail.
For each regulator, record unloaded and rated application-load voltage,
startup/turn-off timing, overshoot, ripple and thermal behavior: ______.
Do not infer high-current operation from an unloaded voltmeter reading.

## I2C and mux

Capture `mux hello` ACK/control-mask reply and confirm the mask is unchanged: ______.

Capture root scan with branches OFF, all mask readbacks, all channel inventories,
and final OFF readback. Test isolation with `mux expect` for a unique branch
address. Repeated addresses across disconnected branches are expected for
identical sensors; root addresses remain visible in every selected channel.

| Channel | Intended endpoint | Fitted device/address | ACK/identity/data | Isolates OFF | Result |
| --- | --- | --- | --- | --- | --- |
| 0 | DFGAS1 | | | | |
| 1 | DFGAS2 | | | | |
| 2 | DFGAS3 | | | | |
| 3 | DFGAS4 | | | | |
| 4 | MS5803 (CSB strap) | | | | |
| 5 | I2C_5 terminal | | | | |
| 6 | I2C_6 terminal | | | | |
| 7 | I2C_7 terminal | | | | |

Verify actual mux straps, pull-up voltages, SDA/SCL rise/fall times and off-state
backfeeding. Test recovery with a controlled fault, then restore normal wiring.
TCA_RESET is not Feather-controlled. Sensor ACK alone does not identify a model
or establish valid pressure/gas readings; exercise each fitted device with its
own driver/reference measurement before final acceptance.

For channel 4, run `ms5803 hello` / `ms5803 read` and record:

- Fitted MS5803 range variant (driver requires -02BA), CSB/address: ______
- PROM CRC4 and fresh raw D1/D2 values: ______
- Loom temperature C / pressure mbar / trusted reference and error: ______
- Previous mux selection restored, repeated reads, absent sensor recovery: ______

The sensor check does not depend on an SD card. Include its output in
`baseline nosd` evidence. ACK/PROM alone is not model identification.

## DS3231 and sleep

Record calendar/OSF/tick, 3-second first alarm, cleared HIGH, 5-second replacement,
A1F and alarm/control restoration. Check repeated `sleep 10s 3` cycles. Every cycle
uses the native Hypnos API and a real DS3231 IRQ. Printed return elapsed also includes modem restart after wake; there is no internal-timer fallback.

| Cycle | RTC before/after | D12 LOW/A1F | Cleared HIGH | Rails asleep/wake | Sleep current | USB return | Result |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | | | | | | | |
| 2 | | | | | | | |
| 3 | | | | | | | |

Capture rail discharge/recovery with a scope. Verify alarm rearming after a
stale flag, manual RTC_INT wake behavior, and battery-backed time retention
across removal of main power. Record CR1220 voltage and a longer timed drift
comparison: ______. The original manual rail settings restore after sleep; repeat peripheral smoke tests after wake.

## SD and SPI

- Card model/capacity/filesystem: ______
- `sd read`: directory and selected retained JSON packet contents: ______
- `sd save` / `sd test`: real MS5803 Loom JSON, retained file, printed byte-for-byte readback: ______
- Verify an existing reference file is unchanged: ______
- SPI loopback: 100 kHz / 1 MHz / 4 MHz, each mode 0-3: ______
- Remove loop jumper; repeat SD test: ______
- Scope SCK, CS, MOSI/MISO edge levels and bus contention with fitted slaves: ______
- Card removal/power-cycle/remount behavior and corrupt/missing card result: ______

## SARA-R5

Continuity must follow actual direction, not the apparent names:
Feather TX -> U4 -> SARA TXD pin 12 **input**;
SARA RXD pin 13 **output** -> U4 -> Feather RX.

- UART 3.3 V/1.8 V domains, OE low before V_INT and high afterward: ______
- A5 gate pulse and physical PWR_ON waveform, module boot time: ______
- TP20 unloaded/minimum during boot and RF burst / ripple / current: ______
- V_INT before/after (actual meter/scope): ______
- `lte hello`: single AT / terminal OK, no power pulse or registration change: ______
- `lte on` alias: same A5/Q1 pulse as `lte pulse`, no scan initiated: ______
- `lte scan`: fitted antenna, CFUN/SIM state, RSRP/RSRQ and operator list: ______
- Scan timeout/cancellation, late terminal response and subsequent AT recovery: ______
- AT/model/firmware/SIM/signal/registration/context query log: ______
- Controlled graceful shutdown/restart using documented modem procedure: ______
- Explicit `lte loom` network smoke (last), APN / carrier / remote payload echo: ______
- GNSS antenna configuration / explicit power-on / time-to-fix / position accuracy: ______

Registration is separate from IP payload delivery. GNSS power status is separate
from a valid fix. Verify UART hardware flow control/default mode if AT is silent.
The schematic RTS/CTS tie and U4's automatic direction behavior must be checked
against the fitted module configuration and waveforms.

## SDI-12 and analog

- Sensor model/address, J_SDI12 12V under load, translator 5V supply: ______
- `sdi <a>I!` identity / `sdi measure <a>` full M/D sequence / reference values: ______
- Manual M! wait + all Dn blocks, returned vs expected count: ______
- Scope break/mark/1200-baud inverted 7E1, DIR turnaround and line loading: ______
- Missing sensor, wrong address, timeout and receive cleanup: ______

For each analog input, apply known GND/midscale/fullscale voltages separately.
Record 64-sample min/max/mean, meter voltage, error and noise; assess crosstalk
while LTE and boosts operate. A3/A4/A5 and D9 are not available analog inputs.

| Input | GND | Midscale | Fullscale <= ADC VDD | Noise/error | Result |
| --- | --- | --- | --- | --- | --- |
| A0 | | | | | |
| A1 | | | | | |
| A2 | | | | | |

Final accepted features / outstanding blocks / evidence links: ______

## November 4 console acceptance additions

These are physical tests to perform after flashing; host checks do not fill them.

- From rails OFF, mux/MS5803/RTC/SD/LTE command automatically acquires power: ______
- LTE first use adopts AT-ready modem or pulses if silent; explicit on repeats: ______
- Modem GPIO1/pad16 mode 2 and GPIO2/pad23 mode 10 verified; LED behavior: ______
- RTC compile-time fallback, ISO/Unix output and explicit timestamp override: ______
- SD Hypnos interval/timezone loaded; `sleep set` override; `sleep save` backup and reload: ______
- Basic Hypnos API sleep `sleep 10s 3`, `sleep 1 min`, actual IRQ callback/elapsed RTC/INT release: ______
- Saved JLT JSON contains correct identity/timestamp/MS5803/TestId and matches printed readback: ______
- `sd list` prints root file names/sizes/folders without opening file contents: ______
- LTE data connection + actual ping reply/RTT, without MQTT: ______
- MQTT broker ACK and exact readback; device/topic/TestId: ______
- **MongoDB manual check** for that TestId and measurement timestamp; insert observed: ______
- RGB on/off and color r/g/b/yellow; correct DIN pin/pixel count and physical colors: ______
- RGB test with stable A0/A1 circuits: 5 s quiet BEFORE, 5 s pulsing, 5 s quiet AFTER: ______
- Per-phase A0/A1 sample count, average/min/max/standard deviation and baseline shifts: ______
- A0/A1 reference/divider values, ADC reference estimate, scope comparison and noise budget: ______
- RGB state and normal ADC reads restored after the test: ______

Current NeoPixel data pin: A2. A later D13 connection shares Hypnos's status-LED GPIO.
Broker readback is not a database receipt; record the actual Mongo observation above.
