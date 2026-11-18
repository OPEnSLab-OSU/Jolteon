# SAMD21 verification firmware

Built from the source and default `config.h` in this package. See ../README.md for testing and LED patterns. This is the original PCB pin profile: invalid hardware SPI clocks are reported as FAIL; software SPI requires explicit selection. Automatic nRF power and extra TX/RX LEDs require configuration after hardware checks/wiring.

`firmware.with_bootloader.bin` starts at 0x0000 and contains the official Zero bootloader, padding to 0x2000, then the sketch. `firmware.sketch.bin` starts at 0x2000 and requires the bootloader already at 0x0000. Use the included tooling; no device was flashed during this build.

Build uses 77652 bytes flash and 19804 bytes static RAM. BUILD_MANIFEST.json records checks, source hashes and image addresses; SHA256SUMS.txt records image hashes.
