# Verification performed

October 13, 2026. No board was flashed or physically tested during this update.

- Final default sketch compiled with Arduino SAMD 1.8.14; OpenOCD configuration checked without hardware access.
- Optional revised SPI/SDI/LED/power profile and stock-compatible pin profile compiled.
- Host tests passed for strict reply/calendar parsing, numeric overflow, known CRC32 vector, immutable retries, stale ACK rejection, remote-source capture, bounded queue overflow/replay, timer wrap and client-independent indicators.
- Host tests passed with extra LEDs disabled and enabled.
- Actual SDI driver passed simulated TX/RX, parity/framing/address, timeout/buffer-bound, DIR-release and independent power-control tests.
- Conflicting SDI power / LTE RX allocation was rejected at compile time.
- Combined image and application vectors checked; stock bootloader CRC independently checked against the binary.

Remaining bench work: actual LED visibility, module power ramp/off isolation, peripheral buses and card, radio/BLE/Wi-Fi peers, RTC waveform, battery calibration, LTE SIM/registration/data, and SDI electrical/cable behavior. Expected SPI map failures, disabled nRF automatic power and blocked original SDI circuitry are explicit defaults.
