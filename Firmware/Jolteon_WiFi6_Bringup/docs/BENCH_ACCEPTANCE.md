# Jolteon bench acceptance

Updated October 14, 2026. **J18 SWD startup passed; peripheral/electrical acceptance remains open.**
Build/configuration/host-test results are in [REVIEW_AND_VALIDATION.md](REVIEW_AND_VALIDATION.md).
Electrical findings and proposed revisions are in [HARDWARE_ISSUES.md](HARDWARE_ISSUES.md).
Command details are in the [verification sketch README](../sketches/Jolteon_Feature_Verification/README.md).

## Recorded user bench result — October 14, 2026

After reflashing the configured crystal-less bootloader and repaired
`SAMD21J18_Blinky` application, the user's D → 1 full SWD probe passed on
SAMD21J18A (256 KB flash / 32 KB RAM), using ST-LINK V2J29S7 and software reset.
The routine did not use the reset wire, regardless of whether one was attached.
Both installed images matched their saved binaries, BOOTPROT was 8 KB, and
hardware breakpoints reached application entry, setup, loop and loop again.
Cleanup resumed the CPU successfully. This is one successful full startup probe;
USB, crystal operation, module interfaces and repeated cold-start reliability
are not established by it.

Bootloader SHA256: `8212f5205a7b779eb8a363fd8e21506fbd92530ef3274369d02ec11719a2b173`.
Application SHA256: `523fcf848c3258bcf70e39a6ab8bef060d2e013e038522a12cf5f23ab8c96314`.
The repaired profile preserves SWD PA30/PA31 and crystal PA00/PA01 during Arduino
GPIO initialization. User-run JSON/log/config evidence is preserved in
[`bench/field-reports/j18-swd-restored-2026-10-14/`](bench/field-reports/j18-swd-restored-2026-10-14/).
The working firmware is retained in
[`releases/samd/crystalless-jolteon/SAMD21J18_Blinky`](../releases/samd/crystalless-jolteon/SAMD21J18_Blinky/README.md).

## Record before testing

Record board revision, rework performed, power sources, firmware SHA256, selected
bootloader, board profile, config.h changes, bridge overlay/build, test date and
operator. Record measured rail voltages and the fitted radio/antenna band.
Keep console logs and instrument captures with the results.

Keep automatic nRF power disabled until its documented power-path checks pass.
Select revised SPI/SDI pin profiles only on matching physical rework. The
bootloader board.json and verification config.h must describe the same board;
they currently require coordinated edits when changing the pinout.

## Test in this order

| Stage | Procedure | Evidence required before calling it passed |
|---|---|---|
| Power and off state | Follow the hardware issue measurement checklist before enabling modules. Test each intended battery/USB/debug-power combination with the switch on and off. | Rail voltages, leakage/current and observed back-power paths recorded; component supply limits respected. GPIO readback alone is insufficient. |
| Minimal startup | Use the PA05-only Blinky first. Observe startup on reset and repeated complete power cycles, with USB/debug cables disconnected where practical. | Heartbeat starts consistently without a serial or wireless client. Record the cycle count and any failures. |
| USB and loader | Test both USB-C plug orientations. Confirm the selected loader's USB monitor/double-tap behavior and native serial in the application. Exercise a native USB sketch upload separately from SWD programming. | Enumeration, upload completion and application restart observed in both orientations; console log and selected image hashes retained. |
| Baseline and visual status | Run `info`, `boot`, `pins`, `all`, `summary`, `led test`, then `led status`. Also observe startup with no client. | Boot identity agrees with the selected image; PA05 patterns match the documented meanings. Each FAIL/BLOCKED result is explained individually. |
| nRF control and UART | After power-path review, use explicit `nrf on`, `nrf probe`, `nrf reset`, `nrf off`. Verify bridge firmware uses RX=P1.05, TX=P1.04. | Measured module rail and reset behavior, UART responses and off-state behavior agree; EN readback alone does not pass power testing. |
| BLE and Wi-Fi | Connect/subscribe a BLE client and configure a UDP receiver. Exercise `ble tx` and `wifi tx <ip> <port>` with their documented token echoes. Test bridge `tx both` from UART, BLE and Wi-Fi in turn. | Each message reaches both other configured destinations, without echoing to its source. Peer roundtrips pass; local ACKs alone do not establish delivery. |
| Reporting and reconnect | Generate `all`/`summary` reports with USB closed, then reopen USB. Disconnect/reconnect the wireless client and inspect `report status`. | Visible reports, replay and counters agree with the documented bounded queue/retry behavior. Record overflow or drops; do not assume lossless logging. |
| I2C, mux and RTC | Run `i2c scan`, `mux test`, `rtc`, `rtc irq`. Attach known devices to the intended mux branches. | Correct addresses/branch selection, advancing time and real IRQ transitions. Confirm pullups and waveforms if results are inconsistent. |
| SPI, RFM69 and SD | Run `spi map`. On current copper, explicitly select `spi soft` for radio/SD tests; after reviewed rework, test hardware SPI separately. Use `radio test`, peer RF tests, `sd read`, `sd list`, then the explicit `sd test`. | Expected map failures remain visible. Register checks, peer packets and SD file roundtrip pass in the selected mode. Keep hardware/software SPI results separate. |
| Battery and general I/O | Compare `battery` with a meter over representative battery voltages; calibrate measured VDDANA/divider values. Use documented free-pin GPIO and SPI loopback jumpers. | Voltage error recorded; changing patterns traverse the actual wires. A static input value is not a loopback pass. |
| LTE | Check supply during module startup/transmit, then `lte probe`, SIM and registration responses. Use the exact installed SARA-R410 model. | AT/model/SIM/registration results and supply captures retained. IP data is a separate test requiring an APN, endpoint and implemented data-session procedure. |
| SDI-12 | Original TXB0102/OE circuit stays BLOCKED. Only after the documented 1T45 rework/profile selection, test identification with a real sensor and inspect DIR/data timing. | Real sensor exchange, released bus after timeout/error and independent sensor power verified. Host waveform simulation alone does not certify the interface. |

The baseline sends no RF packets, writes no SD files and does not activate a
modem data profile. Its completion is not a full board acceptance result.

## Expected current limitations

- Both original SPI clock maps fail hardware mapping checks; dependent hardware
  SPI tests are blocked until rework or explicit software SPI selection.
- Automatic nRF power stays guarded pending the power-path review. Without
  that review/explicit enable, wireless tests remain blocked.
- Charger/solar/cutoff and off-state leakage require instruments. The present
  status wiring does not provide the MCU feedback needed to automate them.
- LTE IP transfer is not implemented by the registration check. SDI-12 on the
  original circuit is blocked. Neither should be marked passed by `all`.
- Pi/macOS system setup, probe access and flashing still need execution on those
  operating systems; their Python/image checks on Windows do not substitute.

## Result record

For every stage, record **PASS / FAIL / BLOCKED / NOT RUN**, the exact procedure,
observed result, log/capture location, and follow-up. A known PCB defect may
explain a failure; it does not turn the feature into a pass. Do not mark the
board fully verified while required features remain blocked or untested.
