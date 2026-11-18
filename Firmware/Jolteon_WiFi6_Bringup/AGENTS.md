# Jolteon maintenance

- This repository copy is the maintained workspace. Preserve `releases/baseline/`,
  `bootloader/upstream/` and import records as provenance. The user requested
  removal of legacy copies. Keep the original parent .ino and separate WT bridge project intact.
- Pin source: `bootloader/board.json`; generator: `bootloader/board_profile.py`.
  The user selected the tested crystal-less configuration as the default on
  October 14; preserve its disabled boot indicator and exact GPIO boot states.
  Original external-crystal defaults remain an optional preset in configs/.
  Also update verification `config.h` and its reviewed driver routes when needed.
  Generated profiles/identity headers come from a successful bootloader rebuild.
- Keep the 8 KB loader/application offset and double-tap cell; application MSP
  must be 8-byte aligned. Retain native USB and the Zero compilation route.
- Current PCB SPI clocks are invalid. Keep explicit failures/disabled hardware
  SPI until a selected revision is physically rewired. Do not enable automatic
  nRF power until the hardware guard is deliberately configured after review.
- Build commands do not flash by default. Only flash when the user's current
  task authorizes programming hardware. No publishing/pushing is implied by builds.
- Menu L maintains sketch-local settings/outputs under `.j18-build/custom-bootloader`.
  Its source defaults to `bootloader/`; isolated `--board-config --out --profile-out`
  builds must not alter the repository defaults or generated default profile.
  Apply clock settings to both bootloader and sketch profile. The menu's `target`
  profile shares the Jolteon FQBN but uses a manifest signature to invalidate caches.
  Preserve compiled `board_config` snapshots for SWD pin aliases and saved menu choices.
- Run `python tools/run_checks.py`; compile affected SAMD board/loader pairs with
  `tools/build_samd.py`; rebuild the bootloader with `--verify-repeat` after its
  source/generator changes. Inspect handoff assembly after modifying MSP/VTOR code.
- Use `tools/build_bridge.ps1` or the documented west sysbuild command for the
  two-core bridge. Preserve source/image manifests and original license notices.
- Source manifest paths must use forward slashes. Honor `.gitattributes`: pinned
  upstream/bridge provenance snapshots retain original bytes; maintained text is LF.
- Record hardware testing separately from build/config/host validation. No
  physical target was tested by the October 13, 2026 repository creation pass.
  The user subsequently passed full J18 SWD startup; see docs/BENCH_ACCEPTANCE.md.
- Keep one-off diagnostics grouped under development/diagnostics. Build scratch
  and routine logs belong in ignored .build folders; preserve useful bench
  evidence in docs/bench and tested configurations/firmware outside those caches.
