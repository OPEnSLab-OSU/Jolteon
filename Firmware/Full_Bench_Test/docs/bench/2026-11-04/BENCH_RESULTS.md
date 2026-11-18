## On-board acceptance — November 4, 2026

Uploaded with verification to Jolteon (COM24). Final application: 208860 bytes.
SHA256: `7cb2531dee8bde0a5f241bd45375f88057af3f155e09ed6a187da6f22a6f6b75`. Serial compile banner: November 4, 13:40:06 Pacific.
The unchanged final image was reflashed once while isolating the mux fault.
`firmware/TESTED_BUILD.json` identifies the flashed image and hashes the evidence.

| Check | Observed result |
| --- | --- |
| Native Hypnos sleep | PASS: one trial plus three repeat cycles; real IRQ callback, released D12, RTC read and USB return. |
| LTE during sleep | Checked Loom CPWROFF before each alarm; native Manager restart after RTC wake. Return elapsed ~22 s includes ~12 s modem boot after a 10 s alarm. |
| DS3231 awake alarms | PASS: fresh 3 s and 5 s matches, A1F, LOW interrupt, clearing to HIGH, configuration restore. |
| Mux + MS5803 | PASS: hello, channel 4 isolation to 0x76, PROM/raw/sensor checks. Full scan issue below. |
| SD | PASS: list; retain actual Loom sample; sync/close/reopen; printed byte-identical readback. Files JLT00000.JSON and JLT00001.JSON retained. |
| LTE data + ping | PASS: repaired R5 internal profile, live 1.1.1.1 echo, RTT 141 ms. |
| MQTT | PASS: live broker QoS 1 acknowledgement and exact current non-retained packet readback. |
| RGB / analog | All 5 s before / 5 s active / 5 s after phases completed through Loom_Neopixel. Comparative INFO, fixture unspecified. |
| SD bypass | Optional SD probes skipped by baseline nosd; the later mux failure is independent of SD. Card was physically present. |
| Deferred | SDI-12 sensor and MOSI-MISO loopback, at the user's request. |

Mongo verification is manual: topic `Wisp_Pole_Test_2026/Jolteon1`, device `Jolteon1`, measured
UTC `2026-11-04T21:43:18Z`, TestId `3E5EEFD850305250462E3120FF0A141F-0002A7CD-0A5F7C3B-1`. Same packet is JLT00001.JSON on SD.
Broker ACK/readback does not establish a database insert.

### Remaining hardware observations

- Full mux scan lost responses after selecting channel 5. I2C clock recovery,
  rail cycling and Feather reset did not restore the mux; a USB power cycle did.
  Cause is not established. Channels 6/7 were not reached. **Do not treat the
  complete mux scan as passing.** Inspect/isolate channel 5 before rerunning
  `mux test` or `baseline`. Final state was recovered, mux mask 0x00, with
  channel-4 isolation, sensor and SD access verified.
- DS3231 again reported OSF/lost power after that USB power cycle, despite the
  earlier user report of a fitted backup battery. Backup retention therefore
  remains unproven. Set current host UTC using the public `rtc set` command;
  the subsequent clock/tick/OSF test passed. No battery voltage was measured.
- Rail GPIO/API transitions and functional wake were tested. Standby current,
  actual rail voltage/discharge and physical LED brightness were not measured.

### Captured final RGB statistics

Values are mV using the configured 3.3 V estimate; A0/A1 fixture and background
LTE activity were not controlled. They are comparative results, not a noise-budget pass.

```text
BASELINE BEFORE / OFF | A0 | samples=22946 | average mV=5.510 | min mV=0.000 | max mV=16.117 | peak-to-peak mV=16.117 | standard deviation mV=1.301 | min/max counts=0/20
BASELINE BEFORE / OFF | A1 | samples=22946 | average mV=1241.736 | min mV=1031.502 | max mV=1462.637 | peak-to-peak mV=431.136 | standard deviation mV=57.116 | min/max counts=1280/1815
PULSING / data active | A0 | samples=7045 | average mV=7.483 | min mV=0.000 | max mV=16.923 | peak-to-peak mV=16.923 | standard deviation mV=1.465 | min/max counts=0/21
PULSING / data active | A1 | samples=7045 | average mV=1305.982 | min mV=997.656 | max mV=1564.982 | peak-to-peak mV=567.326 | standard deviation mV=92.519 | min/max counts=1238/1942
BASELINE AFTER / OFF | A0 | samples=22952 | average mV=5.337 | min mV=0.000 | max mV=16.117 | peak-to-peak mV=16.117 | standard deviation mV=1.323 | min/max counts=0/20
BASELINE AFTER / OFF | A1 | samples=22952 | average mV=1240.534 | min mV=1016.996 | max mV=1447.326 | peak-to-peak mV=430.330 | standard deviation mV=57.533 | min/max counts=1262/1796
```

### Repairs established during this flash/test cycle

- Stale EIC attachment caused an immediate false second alarm: D12 HIGH/A1F clear.
  Atomic attachment, synchronization and guarded callbacks restored repeat alarms.
- One WFI return incorrectly restored rails before the RTC alarm. Native LowPower
  entry now ignores unrelated early wakes and waits for DS3231 assertion.
- Temporary sensor labels were borrowed by Manager JSON. Copying labels into
  the packet fixed the observed SD serialization failure; a regression test covers it.
- R5 internal PSD setup now sets APN and IPv4 before CID 1 mapping/activation,
  bypasses unsafe exact-APN discovery, and retains registration between attempts.
  Live ping and MQTT succeeded after the repair.
- First-use AT probing adopts a modem already running after Feather reset;
  sleep explicitly shuts down modem hardware even before network initialization.

Captured serial logs are under `docs/bench/2026-11-04/`. Logs ending `-05.log`
refer to the final firmware. Earlier logs preserve failed intermediate trials
and must not be attributed to the final image.
