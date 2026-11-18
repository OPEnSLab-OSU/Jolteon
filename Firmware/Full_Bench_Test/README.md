# Jolteon Feather M0 bench console

Open `Loomified_LTE_R5_debug_passthrough.ino`, select **Loomified Feather M0**,
and use Serial Monitor at **115200 baud**, newline or CRLF. Wiring is from
`docs/Jolteon_Rewire.pdf`; see `docs/Feather_M0_Wiring.svg` for Feather connections.

Startup leaves switched rails off. Protocol commands automatically enable the
Hypnos 3V/5V prerequisites; SDI data commands also enable sensor 12V. First LTE
use probes AT and adopts an awake modem; silence triggers one automatic A5 pulse.
A card is optional except for SD and MQTT tests.

The connected board was flashed and tested; see
[bench results](docs/bench/2026-11-04/BENCH_RESULTS.md). Sleep/LTE/MQTT/SD and
channel-4 sensor checks passed. The full mux scan loses communication at channel
5 and requires a USB power cycle; `baseline` also selects that branch. Inspect
that branch before repeating the full scan. RTC backup retention also needs
attention: OSF returned after USB power cycling, and current UTC was reset and verified.

## Commands

| Commands | Behavior |
| --- | --- |
| `baseline [nosd]`, `summary`, `help` | Basic checks, optional SD bypass, last results, grouped help |
| `rails on/off/3/5/status`, `hypnos enable/disable/status` | Exercise real Hypnos rail calls |
| `rails 12 on/off`, `sdi power on/off` | Sensor boost control |
| `mux hello`, `mux test`, `i2c scan/recover` | Read mux mask, scan eight channels, recover bus |
| `mux addr 113`, `mux expect 4 119` | Select mux address; check unique branch isolation |
| `ms5803 hello/read [0x76/0x77]` | Channel 4 PROM CRC4, fresh ADC reads, real Loom MS5803-02BA readings |
| `rtc show/test/alarm` | ISO UTC + Unix seconds, tick/OSF test, repeated IRQ/clear test |
| `rtc set compile` | Explicit compile-time reset |
| `rtc set 1793836800` or `rtc set 2026-11-05T00:00:00Z` | Set Unix UTC seconds or ISO UTC |
| `sleep`, `sleep 10s`, `sleep 1 min`, `sleep 10s 3` | Default/explicit interval; optional 1..5 cycles |
| `sleep set 30s`, `sleep status` | Session override and source display |
| `sleep load [file.json]`, `sleep save 1min` | Load SD defaults; persist interval with a verified temporary file and backup |
| `sd save` / `sd test` / `sd write` | Retain a real Loom sensor JSON packet and print/compare its readback |
| `sd read [JLT00000.JSON]` | List files and print a selected/test sample |
| `spi loop` | MOSI-MISO jumper test at three rates and four modes |
| `lte hello`, `lte on` / `lte pulse`, `lte leds` | AT readiness and first-use wake if needed, explicit pulse, LED configuration/readback |
| `lte test`, `lte scan` | Status queries; explicit operator scan up to 180 seconds |
| `lte register [auto/att]` | Conditional registration recovery; preserve existing home/roaming connection |
| `lte connect` / `lte loom` | Adopt awake modem and establish LTE/APN data without another power pulse |
| `lte ping [1.1.1.1]` | Establish data and require an actual internet ICMP echo reply; no MQTT |
| `mqtt test` / `lte mqtt` | SD credentials, saved sensor packet, actual Loom publish, exact MQTT packet readback |
| `device name MyJolteon`, `device instance 1`, `device` | Session device identity; edit config.h for persistent defaults |
| `lte credentials/pins/quiet`, `lte bridge`, `AT...` | Credential presence, wiring, release UART, raw bridge, bounded AT |
| `rgb on`, `rgb off` | Restore/disable stored color |
| `rgb color r/g/b/y/c/m/w`, `rgb color 255,255,0` | Select a color (each numeric component 0..255); turns RGB on |
| `rgb test` / `neopixel test` | A2 pixel activity versus A0/A1 ADC statistics |
| `sd list` | Root directory names, file sizes and folders; contents are not printed |
| `analog A0/A1/A2 [known volts]`, `analog ref 3.300` | ADC known-input comparison and voltage-reference estimate |
| `sdi 0I!`, `sdi measure 0` | Direct SDI-12 frame or complete M/D measurement |

Slash-separated options above are alternatives, not literal slash syntax.
Attach the LTE antenna before RF/network tests. Ctrl+] exits the raw bridge
or ends a long scan/ping wait; a pending modem reply/result must finish before
another transaction. Nothing flashes hardware from these build commands.

## Power and RTC

D5 LOW enables 3VRAIL. D6 HIGH enables 5VRAIL/HIGHVRAIL. The bench build
uses Rewire v9 for 12V_EN on Feather D13 (header position 25). The later
drawing in docs/ routes 12V_EN to A3. Set SENSOR12_EN in config.h to match
the wired board.
The root SDA/SCL pull-ups R9/R10 use switched 3VRAIL, although the mux/RTC
themselves use +3V3. This is why software automatically supplies prerequisite
power. +3V3, 5VOUT and modem VCC_3.8V are always-on. GPIO readback reports
control state, not measured voltage. Hypnos uses **useSD=false**; SD diagnostics
use the console's separate card object. SPI is the Feather route, not old J18.

The RTC seeds from sketch compile time if lost/older and keeps a newer valid
clock. The build timezone is explicitly Pacific with SDK PST/PDT conversion;
an SD timezone change does not reinterpret that build clock. Compile time is
approximate, so `rtc set` can override it. Years 2000..2099 are supported.
Sleep follows Loom's `examples/Hypnos/Hypnos_Sleep`: register the D12 wake
callback, `setInterruptDuration(TimeSpan(...))`, `reattachRTCInterrupt()`,
`hypnos.sleep()`, and `hypnos.wakeup()` in the callback. Hypnos owns alarm setup,
standby, USB and power sequencing. The console reports the actual IRQ callback,
elapsed RTC time and released interrupt pin, then restores prior rails/UART.
`rtc alarm` remains an independent detailed diagnostic; its result does not gate
the basic sleep command. SDK aborts/awake overruns do not count as a passing wake.
Before each interval, the console calls Loom's checked `shutdownForSleep()` /
`power_down()` path (`AT+CPWROFF`), including after AT-only use. The 3.8V LDO stays
powered; the modem itself powers off. Shutdown finishes before the RTC alarm is
armed. Hypnos then disables the switched rails/onboard indicator and restores
them on wake; ordinary Manager power-up restarts LTE. Printed return elapsed time
includes the modem's roughly 12-second restart after the RTC wake.
The SDK rejects stale EIC attachment flags and ignores unrelated early WFI returns
until the physical DS3231 alarm asserts. `rtc alarm` prints register readback,
MCU IRQ, D12 level, A1F, elapsed time and clearing separately.

Each `sleep` cycle saves its absolute UTC deadline and the 3V/5V/12V rail
states in Feather flash. After USB causes a reset, startup holds the switched
rails off and waits in standby on D12 (the DS3231 alarm output). It leaves the
DS3231 time and alarm untouched while waiting. An alarm that fired while main
power was absent is still latched when main power returns, provided the backup
cell kept the RTC running. Startup then enables the I²C pull-ups, verifies the
alarm and deadline, clears Alarm 1, and restores the saved rails. A fallback
timer opens the console with the rails off if D12 never asserts. That timer
waits up to the original interval plus three seconds from reboot. This path
resumes one armed cycle and works without SD.

Bench check: run `sleep 2min`, then disconnect USB with the RTC backup cell
fitted. If a main battery is fitted and the test calls for total main power
loss, disconnect that too. Reconnect before the deadline. Check that the rail
LEDs stay off through boot and turn back on at the original deadline. Repeat
with power restored after the deadline and verify the latched alarm wakes
the board without a second wait.

Default sleep loads `SD_config.json`, then tries `hypnos_config.json`,
`hypnos.json`, `hypnosConfig.json`. Use `sleep load <name>` for another file.
Missing SD/config keeps the **10-second** default. Explicit/session durations
work without SD. Valid test durations total **3 seconds..24 hours**.
Loom-compatible JSON accepts top-level days/hours/minutes/seconds or nested
`SleepInterval`, `sleepInterval`, or `sleep_interval`, plus named timezone:

```json
{"timezone":"PST","SleepInterval":{"days":0,"hours":0,"minutes":1,"seconds":0}}
```

`sleep set` changes this session only. `sleep save` preserves other JSON fields,
writes/verifies a fresh temporary file, and retains the previous config as
`HYPnnnnn.BAK`. An unreadable existing config is preserved.

## Saved samples, LTE and MQTT

Each SD save calls the actual `manager.package()` and `Loom_MS5803::package()`.
The JSON has normal Loom `type`, `id`, `timestamp`, `contents`, packet number,
MS5803 temperature/pressure, and a `Bringup.TestId`. A new **JLTnnnnn.JSON** is
synced, closed, reopened, printed, and compared byte-for-byte. It remains on SD;
existing samples are never overwritten. `sd read` prefers the file just saved; after reset it selects the highest test name seen in its bounded directory listing (up to 256 entries). SD uses FAT16/FAT32, not exFAT, and no
formatting occurs. The read command does not print credential/config contents.
`baseline nosd` performs no card access and keeps the other basic checks running.

LTE first probes `AT` so a modem that survived a Feather reflash is kept awake.
If it is silent, the first-use pulse is A5 HIGH 1.2 s, then LOW, then a 10-second boot wait;
Q1 pulls actual SARA PWR_ON LOW. Later hellos do not pulse again. `lte on`
explicitly repeats a pulse; skip it when the modem is already answering AT.
This pulse alias is not an idempotent power-state setter. The module may still
perform its own saved/default registration behavior after power-on.

LED setup reads stored modes, changes only mismatches, moves duplicate status
assignments, then verifies **GPIO1/pad16 = network status mode 2** and
**GPIO2/pad23 = module status mode 10**. UGPIOC uses SARA physical pad IDs,
not Feather pin numbers. The installed Loom startup did not configure these
board-specific LED assignments.

`lte test` remains a status check; `lte connect` and `lte ping` separately test
registration/data and enable normal RF mode if needed. Ping requires an actual +UUPING reply with nonnegative RTT;
the initial command OK is insufficient. ICMP may be filtered by a carrier/host.
`lte scan` shows reception/operator visibility, not definitive antenna continuity.
SARA UART uses DCE names: Feather TX -> SARA TXD input; SARA RXD output -> RX.

MQTT reads existing **mqtt_creds.json** with standard Loom keys `broker`, `port`,
`database`, optional `username`, `password`, `project`, without printing them.
It uses the actual **Loom_MongoDB** topic `[project/]database/NameInstance` and
QoS 1 publish. It saves the same packet on SD, subscribes before publishing,
then compares a non-retained readback with the exact current packet. Subscription
permission/broker policy can prevent readback independently of publish ACK.
The console prints device/topic/time/test ID/file and tells the operator to
**check MongoDB manually**. Broker ACK and MQTT echo do not confirm insertion;
the offline mock is not evidence about the live service.

Persistent device defaults are **Jolteon / instance 1** in `config.h`.
Cellular APN/auth use existing `arduino_secrets.h` (empty APN falls back to
`hologram`). Neither credential file is overwritten.

## NeoPixel and ADC fixture

`NEOPIXEL_PIN` is **A2**, `NEOPIXEL_COUNT` is 1, and RGB channels are 0..255.
Connect pixel DIN to A2 and share ground; supply the pixel appropriately.
`rgb off` preserves the color; `rgb on` restores it. `neopixel` aliases `rgb`.
On Rewire v9, D13 is reserved for the sensor boost. The sketch disables
Hypnos status LED writes on that pin. The physical Feather LED still
follows D13, and GPIO behavior before the application starts is governed
by the Feather bootloader.

The console uses `Loom_Neopixel`, with only its third port enabled and mapped to
`NEOPIXEL_PIN`; A0/A1 remain analog inputs. `rgb test` samples both inputs for
five seconds in each phase: pixel OFF/data quiet baseline **before**, changing
full-scale RGB/white/OFF frames, then pixel OFF/data quiet baseline **after**.
Allow about 15 seconds total, plus brief settling time. The ADC free-runs
during each transmitted frame and its latest completed result is read at frame
end. This compares coupling/load effects, not every edge like a scope capture.
ADC configuration and previous RGB state are restored. Each phase reports sample
count, average, minimum, maximum, peak-to-peak and sample standard deviation in
mV, plus raw min/max counts. It also reports pulsing-minus-before and
after-minus-before average shifts. mV uses the configured ADC reference estimate.
Connect A0/A1 to stable reference/test circuits; floating inputs do not establish
circuit noise. Comparative results are INFO until an input/noise budget is chosen.
Keep analog inputs within the Feather's 3.3V range; A2 cannot simultaneously be
an independent analog input while wired to pixel DIN.

## Build and validation

```text
python tests/run_tests.py
python build.py --firmware-dir firmware/sleep-resume
```

Uses Loomified SAMD 4.9/Loom 4.9.1, Loom_Neopixel (backed by the bundled Adafruit
driver), and ArduinoMqttClient.
The SDK patch at `docs/loom-sdk-bench.patch` includes the checked absolute
UTC alarm used for sleep resume and the Hypnos status LED option. Preserve the SDK updates when moving this
sketch to another PC.
The R5 adapter also fixes internal profile setup: profile 0 is explicitly given
the APN and IPv4 protocol before mapping to the CID 1 that Loom configures and
activating it. It bypasses the bundled TinyGSM APN search, which cannot safely
handle carrier-expanded APNs such as `hologram.mnc042.mcc240.gprs`. Each stage is
checked and the internal profile must return an IPv4 address. Cellular
registration is retained between R5 data attempts. Authentication, when supplied,
uses R5 `UAUTHREQ` on CID 1. Live data, ping, broker acknowledgement and MQTT
readback passed in the recorded bench run. Mongo insertion remains a manual check.
The command order follows the [u-blox R5 AT manual, sections 14.6â€“14.7](https://cdn.sparkfun.com/assets/b/6/2/4/5/SARA-R5_ATCommands__UBX-19047455_.pdf).
The sleep-resume image and its manifest are in `firmware/sleep-resume`.
The parent `firmware` folder contains the November 4 bench-tested image.
The build verifies offset 0x2000, MSP 0x20007FF8, double-tap cell 0x20007FFC.
Build/host checks do not flash or replace physical acceptance; record actual
bench evidence in `docs/BENCH_ACCEPTANCE.md`. The original sketch and credential
header remain preserved.

### Swap and read sensors

- Run `i2c scan 2` to scan only mux channel 2. Upstream addresses are marked
  separately. Replace 2 with the connector under test.
- Run `dfgas read 2` for the DFRobot CO board at 0x74 or 0x75, or specify the
  address explicitly with `dfgas read 2 0x75`. The firmware verifies the
  response checksum and CO type before reporting ppm. A fresh bench reading
  checks communication. Follow the sensor warm-up and calibration procedure
  before interpreting concentration.
- Run `soil read 2` for the Adafruit STEMMA seesaw soil sensor. The default
  address is 0x36. The output is temperature and raw capacitance counts.
- Run `sdi set teros11 0` or `sdi set teros12 0`, then `sdi read`. Use
  the sensor's actual SDI-12 address. Selection is session-local. The test
  checks the reported model and returns water-content counts and temperature,
  plus bulk EC on the TEROS 12. `sdi 0I!` and `sdi measure 0` remain available
  for identification and generic raw measurements.
- Run `teros10 read A1` for a TEROS 10 connected to A1 and powered from a
  suitable excitation supply. Substitute the ADC pin you actually wired.
  The output is raw ADC counts and voltage.
- Run `tds read` for the TDS meter on **A0**. The bench output is counts
  and voltage. Converting to ppm requires the meter's transfer function and
  calibration.
