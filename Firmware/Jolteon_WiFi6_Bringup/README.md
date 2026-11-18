# Jolteon bring-up firmware

A standalone workspace for the ATSAMD21J18 Jolteon controller and WT02E40E
Wi-Fi/BLE bridge. It contains current source, a custom SAM-BA loader and Arduino
profile, portable flashing tools, hardware findings/references, tests, and
clearly labelled firmware releases. The parent folder now contains the original
`SAMD21J18_Blinky.ino` and this repository. Legacy copies were removed at the
user's request. Nothing was flashed during this review.

## Start here

Open **`Jolteon_Bringup.cmd`** on Windows, **`Jolteon_Bringup_Linux.sh`** on
Linux / Raspberry Pi, or **`Jolteon_Bringup_Mac.command`** on macOS.
The menu labels the four flash actions explicitly; **P** chooses Arduino Zero
Native USB or Jolteon J18, **S** chooses a sketch, and **B** chooses a bootloader.
**T** switches to build/check only; **R** explicitly recompiles without flashing.
The menu remembers your sketch, board, bootloader and mode across sessions.
Fresh menus default to the tested crystal-less Jolteon board profile and loader,
with the boot indicator disabled. Existing saved selections are retained.
**L** opens detailed bootloader settings for the selected sketch, using our Jolteon
source folder by default. It builds a matching loader and Arduino profile, including
external-crystal or crystal-less clock mode, without changing repository defaults.
**G** rebuilds the repository default loader/profile used by **B option 4**, then
selects that matching pair. It uses `bootloader/board.json`, rather than L's
sketch-local settings, and does not flash.
**E** builds/installs/exports Arduino profiles as a single workflow; **E → 5**
rebuilds the default, installs it and creates a shareable profile ZIP. The direct
command is `python tools/arduino_profile_workflow.py --build --install --export`.
See [profile releases and Loom distribution](docs/ARDUINO_PROFILE_RELEASE.md).
Each sketch keeps one static `.j18-build/firmware` folder. Existing app binaries
are reused until you rebuild or delete them. See
[the menu guide](docs/BUILD_AND_FLASH_MENU.md) for every action and platform setup.

Board-profile conflicts now recompile automatically in that same static folder.
**D** runs the [SWD probe](docs/SWD_PROBE.md): chip/CPU state, flash comparisons,
GPIO/pin mux and SERCOM snapshots, plus optional startup checkpoints. It does
not program flash; it pauses the CPU and the full routine uses software reset.
It auto-detects supported SAMD21 E/G/J chips, including Feather M0 G18 and
Jolteon J18. D → 3 provides generic inspection without local-image comparisons.
Startup reports include clock-ready state and matched-ELF function locations.
**D → 5** performs full image/startup checks with wired ST-LINK reset. **D → 6**
identifies installed bootloader/sketch bytes among local compiled images, with a
choice of ordinary or wired-reset connection. HardFault reports capture the
MSP/PSP exception frame and active exception vector when readable.
Flash action 1 uses the board profile's loader; action 2 uses B's selected loader.

**Q** [installs/updates custom Arduino board profiles](docs/ARDUINO_PROFILE_INSTALL.md)
for use in Arduino IDE/CLI. The installer supports other hardware-profile folders
and an editable profile catalog for future board/bootloader revisions.

The commands below are the separate release-building workflow; it produces
manifested releases rather than using the menu's static application cache.

Use Python 3.10+ and Arduino SAMD core **1.8.14**. The SAMD helper finds Arduino
CLI on PATH or in the standard Windows Arduino IDE installation. If necessary:

```text
arduino-cli core update-index
arduino-cli core install arduino:samd@1.8.14
```

From this repository in PowerShell:

```powershell
py -3 tools/build_samd.py blinky
py -3 tools/build_samd.py verification
```

On Linux/macOS, use `python3` instead of `py -3`. These commands compile,
validate image layout/source hashes, and check OpenOCD configuration when
OpenOCD is installed. **They do not flash by default.** `blinky` only toggles an
external PA05 LED with a series resistor; it does not power modules or wait for
a USB/BLE/Wi-Fi client. `verification` runs the full interactive hardware checks.

For the known manual Zero route:

```powershell
py -3 tools/build_samd.py verification --board zero --bootloader official
```

For Zero-compiled code with the custom Jolteon loader:

```powershell
py -3 tools/build_samd.py verification --board zero --bootloader jolteon
```

The helper always provides the **selected bootloader's actual byte count/CRC** to
the verification sketch and disables conflicting core USB LEDs. It does not
guess a loader from an old build folder. Its default is the Jolteon profile and
Jolteon loader. It uses the repository's board profile without globally
installing it. Explicit tool paths are accepted with `--arduino-cli`,
`--arduino-package`, and `--openocd`.

## Flashing

Install OpenOCD 0.12+ and the appropriate ST-LINK USB driver. Connect target
power normally, SWDIO to PA31, SWCLK to PA30, and common ground. The supplied
configs use software reset; no reset wire is required for this workflow.

```powershell
py -3 tools/build_samd.py verification --flash
```

That explicitly programs the selected loader at **0x00000000** and fresh
sketch-only image at **0x00002000**, verifies both, restores the 8 KB BOOTPROT
region, and runs. On a Raspberry Pi **4** using BCM GPIO SWD:

```sh
sudo python3 tools/build_samd.py verification --adapter gpio --flash
```

BCM8 / physical pin 24 is SWDIO; BCM11 / physical pin 23 is SWCLK. This GPIO
configuration is not for Pi 5. Other Linux/macOS machines can use ST-LINK.
Prefer building as the normal user with GPIO/device access configured; running
under sudo may require explicit `--arduino-cli` and `--arduino-package` paths
because root has a different Arduino installation.

For boot-only flashing or prebuilt files, see [bootloader/README.md](bootloader/README.md).
The OpenOCD configs accept `J18_CHECK_ONLY=1` for file/config validation without
opening the probe. They also reject a combined image supplied as an app.

## Layout

```text
bootloader/                       Custom loader source, board.json, profile, tests
sketches/Jolteon_Blinky/           Safe PA05-only heartbeat
sketches/Jolteon_Feature_Verification/
                                  Peripheral tests, configuration, UART reports
bridge/                           WT02E40E source snapshot and BLE web console
tools/build_samd.py               Explicit board/loader build and optional flash
Jolteon_Bringup.cmd               Windows build/flash selector
Jolteon_Bringup_Linux.sh          Linux / Pi build/flash selector
Jolteon_Bringup_Mac.command       macOS build/flash selector
tools/build_bridge.ps1            Nordic dual-core build with configurable SDK paths
tools/run_checks.py               Host regression checks
tools/*_J18_Tools/                Self-contained Windows/Linux/macOS compatibility tools
docs/                             Hardware findings, references, import/review records
releases/samd/                    New SAMD releases with build manifests
releases/bridge/                  Imported and freshly built bridge images
releases/baseline/                Previous validated outputs, preserved as history
sketches/SAMD21J18_Blinky/        Copy of the original parent sketch
.build/                          Ignored compiler scratch/cache/logs
```

The original exploratory blinky automatically powers the WT module, uses older
commands and LED choices, and has unbounded UART waits. It is retained for
reference; use `sketches/Jolteon_Blinky` or the verification sketch for bring-up.

## Board profile and next PCB revision

For Arduino IDE, install the generated local profile into your configured
Sketchbook location:

```powershell
py -3 bootloader/install_profile.py --sketchbook "$env:USERPROFILE/Documents/Arduino"
```

Restart IDE and select **OPEnS Jolteon SAMD21J18 (Native USB)**.
FQBN: `opens:samd:jolteon_j18`. Named pins are listed in
[bootloader/PIN_ALIASES.md](bootloader/PIN_ALIASES.md). Pin numbers differ from Zero.
The profile provides native USB, the two module UARTs, Wire, GPIO and ADC/DAC;
PWM metadata is not implemented. UART initialization does not automatically
power the modules. Use the sketch's reviewed power sequencing.

For the next revision, edit **bootloader/board.json**, then rebuild:

```powershell
py -3 bootloader/build.py --verify-repeat
```

Rebuild the sketches and reinstall the profile afterward. Change
`bootloader/board_profile.py` and the verification driver's route checks when a
SERCOM mapping changes beyond the reviewed options. Keep application origin
`0x2000`, double-tap word `0x20007FFC`, and 8-byte application stack alignment.
The custom profile reserves 8 RAM bytes, putting its initial MSP at `0x20007FF8`.
The builder stages and checks outputs before publishing. The installer refuses
stale/generated profile files and backs up changed files in its own installation.

Current hardware SPI0 and SPI1 clock routing remains invalid. Default hardware
SPI objects are disabled; the verification sketch reports the mapping failures.
Explicit software SPI is available for testing. The proposed rewires and the
unsupported SPI1-only variant are described in the review/hardware documents.

The loader disables the Zero LED assignments on PA17/PA27/PB03, uses PA05 for its
monitor indicator, keeps nRF enable off, deasserts LTE control gates, and holds
chip selects inactive. Malformed app vectors stay in the USB monitor; the
application handoff uses assembly with no C stack access after switching MSP.
USB-C routing/reset/power faults still require physical corrections.

## WT02E40E bridge

`bridge/` is a source snapshot of the current separate bridge repository; its
origin commit and import hashes are recorded in [docs/IMPORT_MANIFEST.json](docs/IMPORT_MANIFEST.json).
It includes the Jolteon UART overlay: RX=P1.05, TX=P1.04; P1.06 is the separate
IRQ net. `tx both` forwards from UART to BLE+Wi-Fi, from BLE to UART+Wi-Fi, or from
Wi-Fi to UART+BLE. BLE requires a connected client and notifications enabled.
UDP needs a configured destination; a locally accepted send is not peer delivery.

On the installed Windows Nordic SDK:

```powershell
./tools/build_bridge.ps1
```

Pass `-SdkRoot` and `-ToolchainRoot` for other installations. The reviewed build
uses NCS v3.3.1 / toolchain 936afb6332 and sysbuild for both nRF5340 cores.
From a configured Nordic SDK terminal on Linux/macOS:

```sh
west build --sysbuild -b nrf7002dk/nrf5340/cpuapp -d .build/bridge bridge -- -DEXTRA_DTC_OVERLAY_FILE=boards/jolteon_uart.overlay
```

This SDK produces **separate** `.build/bridge/merged.hex` (CPUAPP) and
`.build/bridge/merged_CPUNET.hex` (CPUNET). Use sysbuild `west flash` to program
both in its declared order, or the copied ST-LINK helper, which explicitly
programs network then application. After the PowerShell build helper succeeds,
`python tools/package_bridge.py` creates
`releases/bridge/repository-build/full_dual_core.hex` by checking both vector
bases, rejecting address conflicts, and verifying the HEX round-trip. A single
application `.bin` or the SDK's `merged.hex` is not the complete module image. See
[bridge/WT02E40E_COMMANDS.md](bridge/WT02E40E_COMMANDS.md) and
[bridge/JOLTEON_UPDATE.md](bridge/JOLTEON_UPDATE.md) for commands and wiring.

The packager requires the helper's successful build receipt, and rejects changed
source, helper code or compiler output. An ad hoc `west build` is usable with
`west flash`, but does not create this release receipt. Keep the two-core build
directory together when using that route.

## Portable copy-in tools

To copy a self-contained tools folder into another Arduino sketch:

```text
python tools/copy_tools.py windows /path/to/Sketch
python tools/copy_tools.py linux /path/to/Sketch
python tools/copy_tools.py mac /path/to/Sketch
```

The copied launchers search their folder and parent for an .ino, check/install
their documented CLI dependencies, and show compilation progress. Windows can
install a matching signed ST-LINK driver from an already available official
driver package; its README links ST's download when needed. Linux chooses Pi 4
BCM GPIO when supported, otherwise ST-LINK. macOS uses ST-LINK/Homebrew.
These compatibility launchers compile Zero and default to the official loader;
use `tools/build_samd.py` for automatic custom-loader checksum selection.
Official downloads/core versions are pinned to Arduino SAMD 1.8.14 and checked.
The Windows separate-bootloader launcher and top-level menus keep one static
`.j18-build` per sketch. They compile when the app is missing, explicitly
rebuilt, or the selected board profile differs from its receipt. Source changes
alone do not trigger compilation. Input hashes record the actual build.
Use the top-level menus for the same persistent behavior on every platform;
the older copy-in Unix helpers use their own per-run build workflow.

## Validation and hardware status

Repository source and maintained tooling live in `bootloader/`, `bridge/`,
`sketches/`, `tools/` and `tests/`. Documentation/reference material is in `docs/`;
curated firmware and provenance snapshots are in `releases/`.
Historical one-off smoke helpers are grouped in
[`development/diagnostics`](development/diagnostics/README.md).
Compiler scratch is ignored under `.build/`; local menu choices and each
sketch's static `.j18-build/firmware` and configured loader remain local.

The working crystal-less J18 settings are saved in
[`bootloader/configs/jolteon-j18-crystalless-tested.json`](bootloader/configs/jolteon-j18-crystalless-tested.json).
Its [tested firmware pair](releases/samd/crystalless-jolteon/SAMD21J18_Blinky/README.md)
and [bench result](docs/BENCH_ACCEPTANCE.md) are retained independently of caches.

```text
python tools/run_checks.py
```

Requires Visual Studio C++ Build Tools on Windows or host C/C++ compilers on
Linux/macOS. Tests cover the actual boot GPIO code and app-vector guards,
protocol parsers, telemetry retries/overflow, client-independent LED patterns,
SDI TX/RX/error/timeout cleanup, invalid pin-profile rejection, and bridge routing.
See [docs/REVIEW_AND_VALIDATION.md](docs/REVIEW_AND_VALIDATION.md) for actual results
and [docs/HARDWARE_ISSUES.md](docs/HARDWARE_ISSUES.md) for electrical limitations.
Compilation and simulated tests do not establish physical USB/radio/power behavior.
Use [docs/BENCH_ACCEPTANCE.md](docs/BENCH_ACCEPTANCE.md) to record the staged
board tests and distinguish expected current-PCB failures from verified features.

The available user-supplied Word, BOM and PDF references are in `docs/reference`.
The original temporary `Jolteon_Rewire.pdf` is unavailable; this is recorded in
the import manifest. No different schematic has been silently substituted.

Third-party notices and licenses are retained in their source folders; see
[docs/LICENSES.md](docs/LICENSES.md). This folder is initialized as a local Git
repository, with no commit, remote, or publication performed. Compiler scratch
and duplicate copied tool folders are ignored. `python tools/package_repo.py`
creates a checked ZIP under `dist/` for copying to another computer.
The ZIP excludes `.git` and compiler caches. After extracting, run `git init`
inside `Jolteon_Bringup` if you want a new local repository there.
