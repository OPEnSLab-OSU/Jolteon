# Jolteon SAMD21J18 bootloader and Arduino profile

This is a source rebuild of Arduino's SAM-BA bootloader with Jolteon defaults. The original installed Arduino bootloader is untouched. The release is built from the source included here; it is ready for bench testing and has not been flashed to hardware during this work.

| Image | Flash address | Purpose |
|---|---|---|
| `firmware/samd21_sam_ba.bin` | `0x00000000` | Jolteon bootloader only |
| A compiled sketch-only `.ino.bin` | `0x00002000` | Arduino Zero or Jolteon application |
| An image made by `combine.py` | `0x00000000` | This bootloader, erased padding to 8 KB, then the application |

The bootloader still supports native USB SAM-BA/bossac, double-tap reset, and the Arduino SAMD core's 1200-baud reset request. The 8 KB boot region, 256 KB flash, 32 KB RAM, and double-tap word at `0x20007FFC` stay compatible with Zero applications. Bootloader firmware itself uses the actual `__SAMD21J18A__` chip definition.

## Jolteon defaults

The generated Arduino profile reserves PA30/PA31 for SWD and PA00/PA01 for the
crystal in both clock modes. Their table entries are `PIO_NOT_A_PIN`, so the
core's all-pin startup sweep and ordinary GPIO APIs leave their mux untouched.
Keep this protection unless deliberately redesigning their use. Older custom
profiles lacked this protection and could disable SWD as soon as the application
started; rebuild the profile and sketch to correct it. Bootloader-only flashing
does not update the application's pin table.

- Boot indicator: **disabled** in the tested default. PA05 is retained as its alias; enabling it uses an active-high SAM-BA pulse and requires an external LED/resistor.
- Remove the Zero's PA17 main LED, PA27 TX LED and PB03 RX LED assignments. PA17 remains LTE RX; PA27 is held low as the RFM69 reset's inactive state; PB03 is left alone.
- Before deciding whether to enter the application, drive nRF enable PA13 low, LTE power/reset MOSFET gates PB30/PB31 low, radio reset PA27 low, and radio/SD chip selects PB13/PB14 low, matching the user's tested configuration exactly. These chip selects are active low: applications must deselect both before sharing their SPI bus. The previous high-CS/external-crystal settings are retained as an optional preset.
- Leave nRF reset/UART/IRQ, LTE UART, bus signals, battery input, and other listed inputs at high impedance without pulls during boot. The bootloader does not automatically power either module. The nRF power-circuit issues documented in `../docs/HARDWARE_ISSUES.md` still require hardware review.
- USB monitor only; the unused Zero UART boot interface on PA10/PA11 is disabled. This does not remove sketch UART support.
- The repository default is now the tested **CRYSTALLESS** configuration, applied to both loader and Arduino profile. PA00/PA01 remain reserved for a future crystal configuration. The former external-crystal defaults are retained in `configs/jolteon-j18-external-crystal-original.json`; import a preset with L → J, then C to build it. See the [configuration menu guide](../docs/BUILD_AND_FLASH_MENU.md).
- Report the revision and version in the SAM-BA version response. No build timestamp is embedded, so repeated builds can produce identical binary bytes.

Native USB still depends on correct physical USB-C routing, CC resistors, power and reset wiring. A bootloader change cannot repair those connections.

## Compiler warnings in the pinned toolchain

The current build can report three inherited warning types: `LITTLE_ENDIAN`
is defined by both the newlib and CMSIS headers; the monitor passes an unsigned
byte-buffer pointer to `strlen`, whose parameter is `const char *`; and an
unused `end` label remains in the monitor. These are compiler diagnostics, not
link failures. A successful build still checks image size/vectors and, with
`--verify-repeat` or menu C/G, requires identical binary bytes from two builds.
That repeat check prints the same warnings twice. The reported 5,348-byte
crystal-less configuration fits the 8,192-byte boot region. These warnings alone
do not identify the cause of a runtime HardFault.

## Flash the bootloader using ST-LINK


Power the target normally at 3.3 V. Connect SWDIO to PA31, SWCLK to PA30, and common ground. These configs use a software reset and do not require the reset wire.

From this folder in PowerShell:

```powershell
& 'C:\Program Files (x86)\OpenOCD\bin\openocd.exe' -s 'C:\Program Files (x86)\OpenOCD\share\openocd\scripts' -f .\openocd_bootloader_stlink.cfg
```

That replaces only the bootloader and preserves the application. It disables BOOTPROT, resets into halt, programs/verifies at zero, restores 8 KB protection, resets into halt, verifies again, and runs.

To flash a sketch-only binary in the same run:

```powershell
& 'C:\Program Files (x86)\OpenOCD\bin\openocd.exe' -s 'C:\Program Files (x86)\OpenOCD\share\openocd\scripts' -c 'set APP_IMAGE {C:/path/to/Sketch.ino.bin}' -f .\openocd_bootloader_stlink.cfg
```

`APP_IMAGE` must be a sketch-only binary linked at `0x2000`. The config checks image sizes and vectors before opening the probe, including rejecting a combined image used as an application. To check files/config without opening a probe, add `-c 'set J18_CHECK_ONLY 1'` before `-f`.

## Raspberry Pi / Linux / macOS flashing

For Raspberry Pi 4 GPIO SWD: BCM8 / physical pin 24 is SWDIO; BCM11 / physical pin 23 is SWCLK. Use common ground and normal target power.

```sh
sudo openocd -f openocd_bootloader_rpi.cfg
```

This requires the `bcm2835gpio` adapter driver and the Pi GPIO layout used by the existing tools. It is not a generic Pi 5 GPIO configuration. Linux/macOS or a Pi using ST-LINK can use:

```sh
openocd -f openocd_bootloader_stlink.cfg
```

An optional application can be supplied before the config:

```sh
openocd -c 'set APP_IMAGE {/path/to/Sketch.ino.bin}' -f openocd_bootloader_stlink.cfg
```

The ST-LINK config selects the direct DAP driver and handles the modern `swd`
and older `dapdirect_swd` transport names. See the upstream [OpenOCD 0.12 ST-LINK
direct-DAP config](https://github.com/openocd-org/openocd/blob/v0.12.0/tcl/interface/stlink-dap.cfg).

## Keep compiling for Arduino Zero

The manual route remains supported: compile `arduino:samd:arduino_zero_native`, flash this bootloader at zero, then the sketch-only `.bin` at `0x2000`.

The existing **separate-bootloader** Windows/Pi/Mac launchers search for a file named `samd21_sam_ba.bin` beside the sketch or in its build folder. Copy `firmware/samd21_sam_ba.bin` beside the intended sketch to opt in. Back up any different file already there. Use the separate-bootloader launcher, or the configs above; the existing combined-image launchers still choose the official Arduino core bootloader.

Changing a bootloader does **not** change Arduino digital pin numbers or the Zero core's USB LED behavior. Zero builds of the feature-verification sketch must continue using its included launchers / `J18BuildOverrides.h`. Named Arduino aliases below require the Jolteon profile.

The verification sketch retains the official bootloader checksum for Zero builds by default. When flashing the custom loader with a Zero build, supply both generated identity macros, or force-include `firmware/JolteonBootloaderIdentity.h` **in addition to** `J18BuildOverrides.h`. Otherwise its read-only boot check correctly reports a different image. The Jolteon profile supplies the custom identity automatically.

## Optional Arduino board profile

The generated platform is in `arduino-profile/hardware/opens/samd`. It references the installed Arduino SAMD core rather than duplicating or modifying that installation. This uses Arduino's [platform referencing mechanism](https://docs.arduino.cc/arduino-cli/platform-specification/#referencing-another-core-variant-or-tool).

Install/update it into your Arduino IDE **Sketchbook location** (`directories.user` for CLI):

```powershell
python install_profile.py --sketchbook '/path/to/your/Arduino-sketchbook'
```

```sh
python3 install_profile.py --sketchbook "$HOME/Arduino"
```

Use your actual configured sketchbook path, especially on macOS. `--dry-run` shows the destination first. The installer refuses an unrelated existing platform and backs up changed files when updating its own installation. Restart Arduino IDE and select **OPEnS Jolteon SAMD21J18 (Native USB)**. CLI FQBN:

```sh
arduino-cli compile --fqbn opens:samd:jolteon_j18 /path/to/Sketch
```

The profile provides:

- `LED_BUILTIN` / `PIN_LED` = PA05; no automatic USB RX/TX LEDs.
- `Serial` = native USB; `SerialWT` / `Serial1` = PA22 TX, PA23 RX, SERCOM3; `SerialLTE` / `Serial2` = PA16 TX, PA17 RX, SERCOM1.
- `Wire` = PA08 SDA, PA09 SCL, SERCOM0.
- `PIN_BATTERY` = PB07 / AIN15; A0..A19 match ADC channel numbers, with DAC0 on PA02.
- `PIN_NRF_EN`, `PIN_NRF_RST`, `PIN_LTE_PWR`, `PIN_RADIO_CS`, `PIN_SD_CS`, and all other functional aliases listed in [PIN_ALIASES.md](PIN_ALIASES.md).
- Physical package aliases such as `J18_PIN_PA05` and `J18_PIN_PB07`. These are Arduino API indices, unlike raw PORT encodings in the bootloader's `src/jolteon_board.h`.
- 248 KB available application flash; reserve the final 8 RAM bytes (marker plus alignment padding); initial MSP is 0x20007FF8.
- Safe output/input initialization in `initVariant()` as well as the bootloader. UART objects do not power modules automatically. Do not begin/transmit on a module UART while that module is unpowered.

**Digital pin numbers differ from Arduino Zero.** Use aliases when moving sketches to this profile. Physical USB, SWD and crystal pins are reserved. This first profile supplies GPIO, ADC/DAC, UART and Wire; PWM metadata is not implemented, so it does not promise Arduino `analogWrite()` PWM on arbitrary GPIO.

**Current PCB hardware SPI remains invalid.** Signal aliases retain the existing PB10/PB11/PB12 and PB16/PB17/PB22 assignments, but `SPI_INTERFACES_COUNT` is zero: calling a default `SPI.begin()` intentionally fails to compile. Explicit software SPI remains possible. The full verification sketch retains its existing mapping-failure reports and manual software-SPI test. If the proposed SPI0 and SPI1 rewiring is entered into `board.json`, the generator enables the corresponding reviewed SERCOM routes. Default `SS` is the SD chip select, including for custom/software SD drivers.

Native USB uploads use the inherited bossac recipe. Install the loader first through the provided OpenOCD config; IDE **Burn Bootloader** is not configured for a probe in this initial profile. The build/flash menu selects Zero, repository Jolteon defaults, or a configured custom Jolteon profile using **P**; **Q** installs a profile for IDE use.

For bench compatibility, the defaults retain Arduino Zero IDs `2341:004D` in the bootloader and `2341:804D` in the application, while changing product strings. These are Arduino's IDs, not an allocation to OPEnS; enter appropriately assigned IDs in `board.json` before product distribution.

## Rebuild and update the next PCB revision

Requirements: Python **3.10+** and Arduino SAMD **1.8.14** with its pinned GCC 7-2017q4, CMSIS 4.5.0 and CMSIS-Atmel 1.2.0 dependencies.

```sh
arduino-cli core update-index
arduino-cli core install arduino:samd@1.8.14
python3 build.py --verify-repeat
```

Windows can run `build_bootloader.bat`; Linux/macOS can run `sh build_bootloader.sh`. The wrappers build only and never flash. If Arduino uses a non-default data directory, pass `--arduino-package /path/to/Arduino15/packages/arduino`.

For the next revision:

1. Update **`board.json`**: revision/version, actual pin assignments, safe output levels, high-impedance functions, indicator and clock options, and USB identity. Keep nRF enable off at boot until the board's power design is validated.
2. For the proposed SPI0 swap, set `SPI0_SCK` to `PB13` and `RADIO_CS` to `PB10`. For proposed SPI1, set SCK=`PB17`, MISO=`PB22`, MOSI=`PB16`. The generator recognizes those routes; do not select them on an unmodified PCB.
3. If a UART/I2C/SPI SERCOM route changes beyond those supported choices, update **`board_profile.py`** using the SAMD21 mux tables. It refuses unreviewed routes instead of silently keeping the old bus setup. Update the verification sketch's `config.h` / driver route checks for the same hardware revision.
4. Run `build.py --verify-repeat`. Review **`CHANGES.patch`**, generated **`PIN_ALIASES.md`**, **`firmware/manifest.json`**, and the size/vector checks. The manifest records source/build-script/compiler hashes, image SHA256/CRC32, layout and revision. Generated headers and profiles should be changed through their source configuration, not edited directly.
5. Run image/config tests with `python -m unittest discover -s tests -p test_images.py -v`. Run `tests/run_tests.ps1` on a machine with Python and Visual Studio C++ Build Tools for the actual GPIO implementation's host test. On Linux/macOS: `c++ -std=c++14 -Wall -Wextra -Werror -Itests/host tests/test_gpio.cpp -o test_gpio && ./test_gpio`.
6. Install/update the profile and compile `examples/ProfileSmoke` and the full verification sketch. Confirm `SERCOM1_Handler` / `SERCOM3_Handler` in the verification ELF are the sketch's strong handlers; the profile UART handlers are weak to allow its custom drivers.
7. Check the OpenOCD config with `J18_CHECK_ONLY`, then bench-test power-on application launch, double-tap entry, USB enumeration, 1200-baud entry/upload, PA05 indicator, module enable/reset/CS states and absence of back-powering. Only mark hardware-tested after those measurements pass.

Source provenance: [ArduinoCore-samd 1.8.14](https://github.com/arduino/ArduinoCore-samd/tree/1.8.14/bootloaders/zero). `upstream/` is the pinned original snapshot with SHA256 records. `src/` contains that source plus the Jolteon adaptation. Original notices and LGPL 2.1 license are included. `build.py` uses the upstream release compile/link options without requiring GNU Make. Upstream CMSIS endian and monitor pointer-sign/unused-label warnings are retained; no new GPIO implementation warnings were observed.

## Explicit combined images

To produce a combined image with this bootloader, rather than relying on an IDE export's bootloader selection:

```sh
python3 combine.py /path/to/Sketch.ino.bin --out /path/to/Sketch.ino.with_bootloader.bin
```

The helper checks both vector tables, limits images to the J18 flash layout, and pads with `0xFF` to **exactly `0x2000`** before placing the sketch. It refuses a combined file passed as a sketch. Flash its output at zero after disabling boot protection, then restore 8 KB protection. The provided two-image OpenOCD flow is the default procedure because the addresses are explicit.

The repository helper `tools/build_samd.py` builds full verification firmware
under `../releases/samd` with matching source/image manifests. See the repository
README for board/bootloader selection. Previous images are clearly labelled in
`../releases/baseline` and are not this revision.
