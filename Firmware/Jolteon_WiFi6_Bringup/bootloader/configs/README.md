# Board configuration presets

`../board.json` is the active repository default. On October 14, 2026 it was
replaced with the user's exact tested crystal-less configuration:
`jolteon-j18-crystalless-tested.json`.

This selects internal clocking for both loader and sketch profile, disables the
boot indicator, keeps nRF/LTE power controls low, and preserves the user's low
radio/SD chip-select boot levels. Low chip selects mean selected devices, not
disabled devices; the application must deselect both before shared SPI use.
All pin aliases and high-impedance assignments come from the saved configuration.
SWD PA30/PA31 and crystal PA00/PA01 remain reserved in every generated profile.

`jolteon-j18-external-crystal-original.json` preserves the previous repository
defaults, including external 32.768 kHz clocking, enabled boot indicator and
high radio/SD chip selects. It is an alternative, not the current default.

Use main-menu G to rebuild the repository default and select it. B option 4
selects this default loader. L → J imports a preset into the selected sketch's
custom configuration; L → C builds that isolated loader/profile, selected by B6.
R rebuilds the sketch. Build commands do not flash hardware.
