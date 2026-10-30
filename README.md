# Yoko-Windhawk-Mods

Source workspace for my Windhawk mods. Each mod is developed as small C++
headers under `mods/<mod-id>/` and assembled into a self-contained
`dist/<mod-id>.wh.cpp` file for the Windhawk mod repository.

## Layout

- `mods/<mod-id>/mod.json` - mod metadata used by the bundler.
- `mods/<mod-id>/src/main.cpp` - lifecycle entry point and include order.
- `mods/<mod-id>/src/metadata.h` - Windhawk metadata, readme and settings.
- `mods/<mod-id>/src/` - settings, hooks, rendering and platform modules.
- `shared/` - header-only helpers shared by mods.
- `tools/bundle.py` - deterministic amalgamation tool.
- `dist/<mod-id>.wh.cpp` - generated source submitted to Windhawk.
- `mods/<mod-id>/doc/` - mod-specific notes and assets.

## Building

Requires Python 3.12 or newer.

```powershell
python tools/bundle.py
python tools/bundle.py --check
python tools/bundle.py --json
python -m unittest discover -s tools/tests -t . -v
```

The bundler starts at `mods/<mod-id>/src/main.cpp`, resolves quoted local
includes relative to the including file, inlines each header once, removes
include guards and `#pragma once`, and keeps angle-bracket includes unchanged.
It validates `mod.json` against the Windhawk metadata block and writes LF-only
output atomically.

Never edit a file under `dist/` by hand. Change the modular sources and rerun
the bundler. `python tools/bundle.py --check` fails when a generated file is
stale.

## Validation

`.github/workflows/validate-mods.yml` verifies the generated bundles are
current, runs the bundler tests, checks strict metadata, and compiles every
bundle with Windhawk 1.7.3 and 2.0.0-alpha.8. It uploads the bundled sources
as the `mod-bundles` artifact and the compiled x86, x64, and ARM64 DLLs as
the `windhawk-<version>-dlls` artifacts.

`.github/workflows/sync-mods.yml` runs after a successful `Validate mods`
push run on `main`. It compares `dist/*.wh.cpp` against `mods/` in
[CYoJkoY/windhawk-mods](https://github.com/CYoJkoY/windhawk-mods) and only
commits and pushes when those files differ. Scheduled runs require a successful
validation of the current `main` commit, and manual runs do the same unless
`dry_run` is set. An up-to-date target is left untouched in every case.

## Adding a mod

Create `mods/<mod-id>/mod.json` with at least `id` and `version`, add
`mods/<mod-id>/src/main.cpp` beginning with the Windhawk metadata include, then
run the bundler. The directory name, `mod.json` id, and the metadata `@id` must
match. Keep mod-specific notes in `mods/<mod-id>/doc/`.
