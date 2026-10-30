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

`.github/workflows/validate-mods.yml` runs the bundler tests, checks strict
metadata, and compiles every bundle with Windhawk 1.7.3 and 2.0.0-alpha.8. It
uploads the bundled sources as the `mod-bundles` artifact and the compiled x86,
x64, and ARM64 DLLs as the `windhawk-<version>-dlls` artifacts.

On a push to `main`, the `bundle` job regenerates `dist/` first. When the
regenerated files match the committed ones, no commit is published and the
sync job is skipped. When they differ, the `compile` job builds the regenerated
artifact, the `publish` job commits only `dist/`, and the `sync` job mirrors
that published commit into `mods/` of
[CYoJkoY/windhawk-mods](https://github.com/CYoJkoY/windhawk-mods). Pull requests
and manual runs keep the strict `python tools/bundle.py --check` gate.

`.github/workflows/sync-mods.yml` is the scheduled and manual fallback. It
requires a successful `Validate mods` push run that covers the checked-out
commit and leaves an up-to-date target untouched. Set `dry_run` to inspect the
target diff without pushing.

## Adding a mod

Create `mods/<mod-id>/mod.json` with at least `id` and `version`, add
`mods/<mod-id>/src/main.cpp` beginning with the Windhawk metadata include, then
run the bundler. The directory name, `mod.json` id, and the metadata `@id` must
match. Keep mod-specific notes in `mods/<mod-id>/doc/`.
