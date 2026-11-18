# WT02E40E / nRF5340 ST-LINK helpers

These helpers target the WT module, not the SAMD21. Both nRF5340 cores must be
programmed. The relocated SDK output has `merged.hex` for CPUAPP and
`merged_CPUNET.hex` for CPUNET; the first file alone is not a complete firmware.

From the repository root after `tools/build_bridge.ps1`:

```powershell
bridge/wt02e40e_openocd_stlink/flash_wt02e40e_stlink_openocd.bat .build/bridge --check
```

That checks image selection only. Omit `--check` to explicitly flash CPUNET first,
then CPUAPP. The helper also accepts `releases/bridge/repository-build` with
`application.hex` and `network_core.hex`.

The normal Nordic SDK path is `west flash -d .build/bridge`, which follows
sysbuild's two-domain flash order. `python tools/package_bridge.py` creates a
checked `full_dual_core.hex` with both vector bases and conflict-free addresses.

The native-DAP OpenOCD path requires nRF53 target support in the installed
OpenOCD build. The legacy HLA files are retained as experimental reference;
they are not a verified nRF53 flashing route. The old mergehex helper is optional;
the repository's Python packager needs no Nordic mergehex executable.

Use normal 3.3 V module power, common ground, SWDIO/SWCLK and the reset connection
specified by the module/debug configuration. Do not assume an ST-LINK clone's
power pin can supply Wi-Fi current. No physical flashing was performed in this
repository review. See ../../README.md for the complete workflow.
