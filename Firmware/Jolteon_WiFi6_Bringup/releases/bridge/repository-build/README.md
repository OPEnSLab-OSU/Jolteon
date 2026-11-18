# WT02E40E repository build

Both nRF5340 cores built from bridge/ with the SDK/compiler recorded in BUILD_MANIFEST.json. No hardware was flashed.

- application.hex: SDK merged.hex; **application core only**.
- network_core.hex: SDK merged_CPUNET.hex; network core at 0x01000000.
- full_dual_core.hex: checked, conflict-free merge of those two files.

Prefer sysbuild west flash with the original build directory, or the repository ST-LINK helper which explicitly programs network then application core. The single application .bin is not complete firmware.
