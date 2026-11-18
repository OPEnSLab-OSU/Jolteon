# Repository review — October 13, 2026

## October 14 update

The user passed a full J18 SWD startup probe with the repaired pin profile;
details and preserved evidence are in [BENCH_ACCEPTANCE.md](BENCH_ACCEPTANCE.md).
At the user's request, that exact crystal-less/boot-indicator-disabled board
configuration is now the repository default. Its loader binary remains SHA256
`8212f5205a7b779eb8a363fd8e21506fbd92530ef3274369d02ec11719a2b173`.
The former external-crystal configuration is retained as an optional preset.
Affected release sketches were rebuilt, and Arduino's installed
`opens:samd:jolteon_j18` profile was updated and compiled successfully using the
IDE's CLI. These installation/build steps did not access the physical target.

One-off development helpers are grouped in `development/diagnostics`; maintained
tests stay under `tests/`. Tested J18 firmware/settings and field reports are
retained outside ignored build scratch. Local static flash files and menu choices
are preserved during repository cleanup. The original creation review below is
historical and describes its earlier validation scope.

The workspace was copied into Jolteon_Bringup. Original SAMD and bridge folders
remain intact. Import hashes and the bridge origin commit are recorded in
IMPORT_MANIFEST.json. No target was flashed or physically tested.

## Corrections made

- **Boot recovery:** invalid/erased app MSP, non-Thumb reset, misalignment and
  entry outside app flash now keep the custom loader in its USB monitor.
- **Stack alignment:** custom profile reserves eight RAM bytes, giving MSP
  0x20007FF8 while keeping the double-tap word at 0x20007FFC. The official loader's
  own word-aligned boot stack is accepted by file checks; applications require
  eight-byte alignment. Zero applications retain MSP 0x20008000 and flash 0x2000.
- **Handoff:** naked ARM assembly avoids C stack access after changing MSP.
  Compiled disassembly, vector argument and VTOR/barrier sequence were inspected.
- **Build consistency:** outputs are staged before publication. Source and
  generated profile hashes are checked before installation/build selection.
  Portable paths and Git text attributes prevent checkout line-ending changes
  from invalidating maintained-source manifests. Upstream snapshots retain bytes.
- **Loader pairing:** the repository helper explicitly selects board/loader and
  supplies that image's actual byte count/CRC to verification. Both board paths
  disable conflicting core USB LEDs. Old build folders do not select the loader.
- **OpenOCD:** relocated Windows backslash paths are handled, SWD speed is set
  after target defaults, and wrong app/combined offsets fail before probe init.
- **Pinned reference:** an earlier combined-image hash was misidentified as the
  official loader hash. Direct verification against ArduinoCore-samd 1.8.14 gives
  6504 bytes, CRC32 032dc51e, SHA256
  `89b9255d2f0bfa90371772b4e2eff78aa6069c6e612eb35737e964074ad8512b`.
  Compatibility downloads/core versions are pinned and checked.
- **Bridge packaging:** SDK merged.hex is CPUAPP-only; merged_CPUNET.hex is the
  network image. The copied helper now requires both and flashes CPUNET first.
  The new packager checks vector bases 0 / 0x01000000, rejects overlaps, and
  verifies a real full_dual_core.hex by decoding it back to the input maps.
- **Release receipts:** source is captured before the bridge build and checked
  afterward; successful output hashes bind the release to those inputs. Changed
  or added source and altered images are rejected. SAMD flashing uses that run's
  frozen output rather than a shared release folder. ZIP inputs are hashed before
  and during writing and verified before the final archive is replaced.
- **Future pins:** unknown SERCOM routes and SPI1-only variants are rejected.
  Current invalid SPI routes remain disabled/reported; no rewiring is implied.
- **Organization:** canonical copy-in tools, safe blinky, full test sketch,
  bootloader/profile, bridge snapshot, available hardware references, tests,
  current releases and historical baselines have separate documented locations.

## Checks performed

- Custom loader built twice identically: 5296/8192 bytes, SHA256
  `6f4b21576c798172ecfc299986c4202a044cf9eedd4820319c324c2389c42b42`,
  CRC32 `3fc39799`; J18 definition and 0x2000 app symbol verified.
- Full verification and safe blinky compiled with the Jolteon profile; full
  verification also compiled for Zero with official/custom loader selection.
  Actual compiler source copies, image vectors and combined layouts are checked.
- Host checks passed with compiler warnings treated as errors: actual GPIO and
  vector predicates, protocol/calendar parsing, immutable telemetry retries,
  stale ACKs, overflow/replay, timer wrap, client-free LED patterns with extra
  indicators disabled/enabled, actual SDI waveform/framing/parity/address and
  timeout/DIR/power cleanup, conflicting SDI/LTE compile rejection, and bridge
  routing/source exclusion/independent destination failures/subscription gates.
- Python checks cover manifests, bad pins/schema states, reviewed/unreviewed
  routes, Linux/macOS vector helpers, official binary, HEX checksums/EOF/overlaps
  and two-core vector requirements.
- Both bridge cores rebuilt with NCS v3.3.1 / toolchain 936afb6332 and RX=P1.05
  overlay. Imported bridge firmware logic was preserved.
- OpenOCD file/config checks ran without opening a probe. Bridge batch
  BUILD_DIR --check selected application.hex and network_core.hex separately.
- Packaging verifies source/image hashes, exact boot+padding+app layout, both
  bridge address spaces, ZIP CRC and archive byte-for-byte content equality.

## October 14 Windows launcher update

The separate-bootloader batch now reuses a successful `.j18-build` compilation
when source/header/data/library/core hashes, board/build settings, CLI identity
and selected toolchain identity/file metadata match. Binary hash/vector checks
reject damaged or incomplete cache entries. `--rebuild` forces compilation;
older builds without a receipt need one initial compilation. Installed toolchain
changes that deliberately preserve file metadata require `--rebuild`.

Its generated configuration selects native ST-LINK directly and tries modern
`swd` before the older transport fallback, avoiding the deprecated interface
file. This changes configuration selection, not the existing bootloader search.
Source staging also prevents Arduino from recursively copying earlier
`.j18-build` runs into a new compiler directory. Per-run build paths and Arduino
clock properties are excluded from the cache key; actual source/settings remain
checked. Cached build-time macros retain their original compilation values.
Host regression checks exercise the actual embedded cache functions, including
changed inputs/settings/dependencies, bad receipts, altered binary, mismatched
board, staging races and toolchain metadata changes. Bench flashing is separate.
Real Windows `--build-only` runs also passed fresh compilation, reuse on the
next run, `--rebuild`, and automatic rebuilding after a source edit. Both
OpenOCD phases checked successfully without the deprecated interface/transport
warnings on the installed OpenOCD. No adapter was opened or target flashed.

Follow-up: that configuration-only test missed a regression in USB discovery.
The native driver also needs the `st-link vid_pid` list from the official
interface. The October 14 correction restores that list in all separate-loader
launchers. A USB-only diagnostic reproduced `open failed` without the IDs and
successfully opened ST-LINK V2J29S7 / 0483:3748 with them. The probe reported
3.2768 V target voltage. No target was defined, reset, erased or programmed by
that diagnostic; this is adapter-access evidence, not board acceptance.

## Remaining bench work

October 14 menu update supersedes the earlier automatic cache-invalidation
behavior described above. The maintained Windows launcher and new native
Linux/macOS menus now reuse one static folder per sketch until explicit rebuild,
board-profile change, or removal. The menu offers combined, separate, boot-only, and app-only actions;
board profile selection is independent from bootloader selection. A cached
application for a different profile now recompiles automatically before programming. Legacy copies
were deleted at the user's request; the original parent sketch remains intact.
See `BUILD_AND_FLASH_MENU.md` for the current controls.

Validation of the earlier menu revision: all 22 Python tests (5 image tests and 17
repository tests) and all required C/C++ host cases passed. Actual Windows menu
builds compiled both Zero and Jolteon profiles; mismatch rejection, explicit
rebuild and static reuse passed. The shared Linux/macOS engine compiled both
profiles on this Windows host and checked all four flash modes through the
installed OpenOCD with `J18_CHECK_ONLY`; no probe was opened. Native shell
syntax and real interactive profile selection were also checked. These are
host/config/build tests; Linux/macOS tool installation and physical flashing
still require testing on their respective machines.

The subsequent SWD revision adds D for inspect-only or startup diagnostics.
The actual Tcl runtime passed eleven simulated-target scenarios covering
normal startup, missing ELF, flash mismatch, invalid vectors, wrong chip,
HardFault, connection/reset errors, checkpoint timeout, inspect-only, and
leaving the CPU halted. CPU/pin/clock/SERCOM snapshots and breakpoint cleanup
were exercised in simulation. Real OpenOCD configuration checks opened no
adapter. ELF reconstruction matched real Zero/Jolteon applications exactly.
Automatic profile changes compiled both profiles through Windows and Unix
engines; a deliberate compilation failure retained the last app and loader.
No target was reset or programmed by the agent while developing it.
See `SWD_PROBE.md` for operation and limits.

Subsequent user bench logs: Feather M0 DID `0x10010305` connected successfully
but hit the initial J18-only guard. The generalized probe now accepts that
G18A identity and known SAMD21 E/G/J variants, checks NVM geometry and uses
detected RAM/flash limits. A J18 user run verified both saved images and reached
the app reset handler, then stopped at PC `0x401A` in `SystemInit`, Arduino core
`startup.c:80`, waiting for XOSC32K readiness. These observations do not establish
peripheral/electrical acceptance or the physical cause of the crystal wait.
No clock defaults were changed. New code captures oscillator/DFLL/GCLK state,
resolves PCs only with a matching installed app/ELF, and distinguishes Zero/Jolteon
stack-vector mismatches from connection failures. D → 3 inspects without local
images or assumed boot/app layout. The data catalog supports future SAMD21
entries; other MCU families need a reviewed register backend.

The updated actual Tcl runtime passed twenty simulated scenarios including
Feather G18A, smaller E14 RAM/flash limits, invalid geometry/IDs, generic
inspection, raw apps at zero and oscillator waits. Full host checks passed
36 Python tests plus the required C/C++ cases. Actual Windows and Unix-engine
configuration checks confirmed combined mode uses the selected board profile's
loader (Zero official, Jolteon custom), independent of B; separate mode retains
its explicit loader selection. This generalized revision has not yet been run
against physical targets by the agent.

The generalized profile installer adds Q and a catalog/source-folder CLI.
Temporary-sketchbook tests cover updates, retaining extra/user-edited files,
idempotence, scoped backups, stale managed-file removal, path validation and
rollback on failed discovery. The real Jolteon profile was installed and
discovered through Arduino CLI in an isolated workspace sketchbook; the user's
global Arduino profile directories were not modified by these tests.

Native USB enumeration/bossac upload, reset/power-on launch, LED/GPIO levels,
wireless peers, physical buses/card/RTC/ADC/SDI, charging, cutoff and off-state
back-powering still require the board. Automatic nRF power remains guarded.
Native nRF53 ST-LINK and Pi/macOS system flashing were not physically exercised;
portable Python validators were tested on this Windows host.

The profile has no PWM metadata and no default hardware SPI for current routing.
IDE Burn Bootloader is not configured; use the explicit OpenOCD workflow.
USB IDs retain Arduino bench compatibility values and need an appropriate
allocation before product distribution.

Five supplied reference files were copied. The original temporary
Jolteon_Rewire.pdf is unavailable; no alternate revision was substituted.
HARDWARE_ISSUES.md retains the existing electrical/BOM findings and suggestions.
