# Yoko-Windhawk-Mods

Source workspace for my Windhawk mods. Each mod is developed as small C++
headers under `mods/<mod-id>/`. The `main` branch holds source code and
tooling; generated bundles live only on generated branches.

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

`.github/workflows/validate-mods.yml` runs the bundler tests and then expands
one `build` job per selected mod. Each build job compiles exactly one bundle
with Windhawk 1.7.3 and 2.0.0-alpha.8 through
`.github/actions/compile-mod`, so x86, x64, and ARM64 builds of one mod are
independent of every other mod. The `bundle` job uploads the bundled sources as
the `mod-bundles` artifact, and each build job uploads its DLLs as the
`windhawk-dlls-<mod-id>` artifact.

On a push to `main`, the `bundle` job generates `dist/` in the runner, each
`build` job validates and publishes its own mod, and the `release` job writes a
snapshot to the `release` branch once every build has finished. The release
root contains only `<mod-id>.wh.cpp` files and a `.gitignore` that ignores
everything else. Each publish replaces the whole snapshot, so deleted mods
cannot leave stale bundles behind. An unchanged snapshot creates no commit. If
`main` moves while compilation runs, the publish is skipped and the newer push
owns the release update.

The `release` branch is the aggregate snapshot: useful for install links and
for anyone who tracks one branch. It is not the submission path, because a
single snapshot cannot become a separate upstream pull request per mod.

## Publishing one branch per mod

The `select` job maps the changed paths of each push to mod ids with
`tools/changed_mods.py`, then emits the id list as the `build` matrix. A push
that only touches `mods/<mod-id>/` selects that mod; a push that touches
`shared/` or `tools/bundle.py` selects every mod, as those inputs can change
any bundle. Pull requests compile every mod so the pipeline is exercised
against all bundles, but never publish. Because the matrix is expanded per mod,
a compile error in one mod fails only its own job; branches for the other
successful mods are still published.

For each selected mod, `publish-mod-branches` writes one commit on top of the
current upstream `main` that replaces exactly `mods/<mod-id>.wh.cpp`, then
pushes it to `mods/<mod-id>` in
[CYoJkoY/windhawk-mods](https://github.com/CYoJkoY/windhawk-mods). The rest of
the tree is the untouched upstream tree, so the branch diff is one file. A
branch already carrying the same bundle on the current upstream tip is left
untouched; a branch on an older base is rebuilt and force-pushed with a lease.
A bundle identical to the upstream file produces no branch at all.

The same action then opens or refreshes the matching pull request in
`ramensoftware/windhawk-mods` with `gh`, inserting the push's first commit line
inside `<!-- changelog:start -->` / `<!-- changelog:end -->`, and adding the
authorship section for a mod that does not exist upstream yet. Because every
mod owns a branch, one mod can be updated, reverted, or submitted on its own.

The default branch prefix, target, and upstream repositories are workflow
environment variables; the action also accepts `mods` (an explicit id filter),
`dry-run`, and `open-prs`.

To publish the branches by hand, run the `Validate mods` workflow from
`main`. `mods` limits the run to some ids, `dry-run` reports the pending
branches without pushing, and `open-prs` is off by default so a manual run
only writes branches in the fork. A push to `main` always opens or refreshes
the upstream pull requests for the mods it touched.

`tools/changed_mods.py` also accepts `--only <mod-id>...` to narrow a selection
and `--matrix` to write the JSON list consumed by the workflow. An empty
selection writes a placeholder entry, because GitHub cannot expand a matrix
with no values.

`SYNC_PAT` needs push access to the fork, plus the `workflow` scope while the
fork's `main` lags behind upstream. A new mod branch is committed on top of the
upstream `main`, so a lagging fork makes the pushed branch carry upstream
workflow commits as well, and GitHub rejects any push that adds or edits a
workflow file without that scope. The action reports such a rejection with
GitHub's own reason instead of retrying it, because the same token is denied
every time. A synced fork needs only push access.

Keep the fork's `main` synced with the upstream repository through GitHub's
**Sync fork** button, or through the API shown below, before publishing. That
keeps the pushed branches free of unrelated workflow commits:

```powershell
gh api -X POST repos/CYoJkoY/windhawk-mods/merge-upstream -f branch=main
```

Opening the upstream pull request needs pull request permission on
`ramensoftware/windhawk-mods` as well; with only push access, the branches are
published and the pull request call fails with the reason `gh` returned.

## Adding a mod

Create `mods/<mod-id>/mod.json` with at least `id` and `version`, add
`mods/<mod-id>/src/main.cpp` beginning with the Windhawk metadata include, then
run the bundler. The directory name, `mod.json` id, and the metadata `@id` must
match. Keep mod-specific notes in `mods/<mod-id>/doc/`.
