# Tested J18 crystal-less blinky

The user flashed this loader/application pair and ran a successful full D1 SWD
startup probe on October 14, 2026. See `docs/BENCH_ACCEPTANCE.md` for the scope
of that result. USB and peripheral/electrical acceptance are still open.

- `samd21_sam_ba.bin`: configured Jolteon bootloader, flash address 0.
- `firmware.sketch.bin`: repaired application, flash address 0x2000.
- `firmware.with_bootloader.bin`: the same pair combined with 8 KB boot padding.
- `firmware.sketch.elf`: matching application symbols for debugging.
- `BUILD_MANIFEST.json`: image hashes and source/configuration snapshot.
- `bootloader-manifest.json`: configured bootloader/profile build provenance.

Sketch source: `sketches/SAMD21J18_Blinky/SAMD21J18_Blinky.ino`.
Trackable settings: `bootloader/configs/jolteon-j18-crystalless-tested.json`.
To reproduce from the menu, choose that sketch, then L → J to import the settings,
C to compile the configured loader/profile, and R to compile the matching sketch.
Those build steps do not flash hardware. The application blinks PA20; the boot
indicator is disabled. SWD PA30/PA31 and crystal PA00/PA01 stay reserved.

The normal menu retains its local working copies in `.j18-build/`; this release
is a portable record and does not replace those local settings automatically.
