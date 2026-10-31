# Yoko-Windhawk-Mods

Source workspace for my Windhawk mods. Each mod is developed as small C++
headers under `mods/<mod-id>/`. The `main` branch holds source code and
tooling; generated bundles live only on the `release` branch.

## Layout

- `mods/<mod-id>/mod.json` - mod metadata used by the bundler.
- `mods/<mod-id>/src/main.cpp` - lifecycle entry point and include order.
- `mods/<mod-id>/src/metadata.h` - Windhawk metadata, readme and settings.
- `mods/<mod-id>/src/` - settings, hooks, rendering and platform modules.
- `shared/` - header-only helpers shared by mods.
- `tools/bundle.py` - deterministic amalgamation tool.
- `dist/<mod-id>.wh.cpp` - local bundler output; ignored by Git.
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
stale. `--output <dir>` writes bundles somewhere other than `dist/`.

## Validation

`.github/workflows/validate-mods.yml` runs the bundler tests, checks strict
metadata, and compiles every bundle with Windhawk 1.7.3 and 2.0.0-alpha.8. It
uploads the bundled sources as the `mod-bundles` artifact and the compiled x86,
x64, and ARM64 DLLs as the `windhawk-<version>-dlls` artifacts.

On a push to `main`, the `bundle` job generates `dist/` in the runner, the
`compile` job validates the artifact, and the `publish` job writes a snapshot
to the `release` branch. The release root contains only `<mod-id>.wh.cpp`
files and a `.gitignore` that ignores everything else. Each publish replaces
the whole snapshot, so deleted mods cannot leave stale bundles behind. An
unchanged snapshot creates no commit. If `main` moves while compilation runs,
the publish is skipped and the newer push owns the release update.

The `sync` job then mirrors that exact release commit into `mods/` of
[CYoJkoY/windhawk-mods](https://github.com/CYoJkoY/windhawk-mods).
`.github/workflows/sync-mods.yml` is the scheduled and manual fallback for the
same release branch. It leaves an up-to-date target untouched; set `dry_run`
to inspect the target diff without pushing.

## Adding a mod

Create `mods/<mod-id>/mod.json` with at least `id` and `version`, add
`mods/<mod-id>/src/main.cpp` beginning with the Windhawk metadata include, then
run the bundler. The directory name, `mod.json` id, and the metadata `@id` must
match. Keep mod-specific notes in `mods/<mod-id>/doc/`.
