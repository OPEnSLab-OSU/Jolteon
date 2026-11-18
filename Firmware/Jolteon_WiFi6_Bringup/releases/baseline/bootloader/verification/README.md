# Jolteon profile verification firmware

Built using opens:samd:jolteon_j18; custom bootloader identity is selected automatically. This is the current PCB pin configuration, with explicit hardware SPI mapping failures and automatic nRF power disabled pending hardware validation.

`Jolteon_Feature_Verification.ino.bin` is sketch-only at 0x2000. `Jolteon_Feature_Verification.ino.with_bootloader.bin` contains this release bootloader at zero, 0xFF padding, and the sketch at 0x2000. Use the parent OpenOCD config with APP_IMAGE set to the sketch-only file. No hardware was flashed during compilation.

Matching source: ../../../Jolteon_Feature_Verification, also supplied in the refreshed companion Jolteon_Feature_Verification.zip. BUILD_MANIFEST.json records source/profile/image hashes.
