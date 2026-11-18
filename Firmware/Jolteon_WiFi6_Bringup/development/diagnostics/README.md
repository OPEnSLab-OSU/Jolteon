# Development diagnostics

`session-scripts/` contains the one-off smoke-test helpers used while developing
the menus, cache handling, board profiles and SWD probe. They are retained as
development history, separate from the maintained tools and automated tests.
`ORIGINS.json` records their original locations under the repository's `.build/`.

These historical helpers assume their original `.build/` location, Windows
tools and/or fixtures from that development session. They are not portable
entry points and are not executed by the menu or test runner. To reuse one,
review it and recreate its fixture prerequisites under `.build/` first. Do not
run it directly from this archive expecting a complete standalone test.

Run `python tools/run_checks.py` for the maintained automated checks. Firmware
build commands and the top-level menus are documented in the repository README.
User-run bench evidence is in `docs/bench/field-reports/`; generated caches and
routine test/build logs belong in ignored `.build/` directories.
