# User-reported bench observations — November 4, 2026

Evidence: serial output pasted by the user in this chat, including older
October 30 firmware and a November 4 build marked 12:55:10 Pacific. These are
user observations, separate from host/build checks and from the latest firmware.

- Feather mux/root scan found DS3231 0x68 and mux 0x71. Mux hello passed.
- MS5803-02BA on mux channel 4, address 0x76: PROM CRC and raw readings passed;
  one reported reading was 21.540 C / 1009.350 mbar. Calibration was not established.
- SD mounted with 31,116,288 sectors. The older 4096-byte write/read/cleanup test
  passed. Later directory output included MQTT credentials and Hypnos config;
  no JLT sample existed yet. The new retained JSON path has not been proven here.
- RTC initially reported lost power and OSF. Later the clock test passed with
  OSF clear, including with a backup battery installed. The separate raw alarm
  diagnostic still failed with the battery. No successful sleep/wake was supplied.
- Modem SARA-R510S, firmware 03.15: AT and SIM READY observed. GPIO pad 16 mode 2
  and pad 23 mode 10 were written and verified by readback. Physical LED behavior
  was not separately established by these replies.
- LTE registered roaming on AT&T; CGATT=1, CGACT context 1 active, carrier-expanded
  Hologram APN and an assigned private IPv4 were reported. Internal data setup
  nevertheless failed on repeated attempts; CEER reported SM activation error
  with no cause. No successful internet ping, MQTT echo or Mongo insert was shown.
- The prior 128-sample RGB test repeatedly showed A0 near 6 mV with roughly
  1–2 mV active mean shifts; A1 near 1.24 V with about 98–123 mV active shifts.
  The input fixtures were not specified, so these do not establish PCB coupling
  or a passing noise budget. The new 5-second before/active/after test needs rerun.

These pasted observations precede this agent's subsequent flash/test/fix cycle.
See VALIDATION.md and the captured bench logs for that separate evidence.
