#!/usr/bin/env python3
"""Map changed repository paths to the mods that must be republished.

A push that only touches ``mods/<id>/`` republishes that single mod. A push
that touches a bundle-wide input, such as ``shared/`` or the bundler itself,
republishes every mod because it can change any bundle.

Usage:
    git diff --name-only "$BEFORE" "$SHA" > changed.txt
    python tools/changed_mods.py --paths changed.txt
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


MOD_PATH_RE = re.compile(r"^mods/([^/]+)/")
DOC_PATH_RE = re.compile(r"^mods/[^/]+/doc/")
MATRIX_PLACEHOLDER = "-"


class SelectionError(Exception):
    """Raised when the changed paths cannot be mapped to mods."""


def known_mods(root: Path) -> list[str]:
    """Mod ids that carry a manifest in the working tree."""

    mods_root = root / "mods"
    if not mods_root.is_dir():
        raise SelectionError(f"missing mods directory: {mods_root}")
    return sorted(
        path.name
        for path in mods_root.iterdir()
        if path.is_dir() and (path / "mod.json").is_file()
    )


def _matches_trigger(path: str, trigger: str) -> bool:
    if path == trigger:
        return True
    return path.startswith(trigger.rstrip("/") + "/")


def select_mods(
    paths: list[str],
    mods: list[str],
    triggers: list[str],
) -> list[str] | None:
    """Changed mods, or ``None`` when a bundle-wide input changed."""

    known = set(mods)
    selected: set[str] = set()
    for raw in paths:
        path = raw.strip().replace("\\", "/")
        if not path:
            continue
        if any(_matches_trigger(path, trigger) for trigger in triggers):
            return None
        if DOC_PATH_RE.match(path):
            # Notes and images never reach a bundle, so they cannot change one.
            continue
        match = MOD_PATH_RE.match(path)
        if match is None:
            continue
        mod_id = match.group(1)
        if mod_id in known:
            selected.add(mod_id)
    return sorted(selected)


def restrict_mods(
    selected: list[str] | None, only: list[str], mods: list[str]
) -> list[str] | None:
    """Limit a selection to explicit ids, rejecting ids without a manifest."""

    if not only:
        return selected
    known = set(mods)
    requested = sorted(set(only))
    unknown = [mod_id for mod_id in requested if mod_id not in known]
    if unknown:
        raise SelectionError(f"unknown mod id(s): {' '.join(unknown)}")
    wanted = set(requested)
    if selected is None:
        return sorted(wanted)
    return [mod_id for mod_id in selected if mod_id in wanted]


def _read_paths(path: Path) -> list[str]:
    try:
        return path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise SelectionError(f"cannot read changed paths: {path}: {error}") from error


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--paths",
        type=Path,
        required=True,
        help="file with one changed repository path per line",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root that owns the mods directory",
    )
    parser.add_argument(
        "--trigger",
        action="append",
        default=[],
        help="path or directory that republishes every mod; repeatable",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="force every mod regardless of the changed paths",
    )
    parser.add_argument(
        "--only",
        nargs="+",
        default=[],
        metavar="MOD_ID",
        help="limit the selection to these space separated mod ids",
    )
    parser.add_argument(
        "--matrix",
        action="store_true",
        help=(
            "write a JSON mod list for a GitHub build matrix; an empty "
            "selection becomes one skipped placeholder entry"
        ),
    )
    parser.add_argument(
        "--github-output",
        type=Path,
        help="append publish_all, mods, and count to a GitHub output file",
    )
    args = parser.parse_args(argv)

    try:
        mods = known_mods(args.root)
        if args.all:
            selected: list[str] | None = None
        else:
            selected = select_mods(_read_paths(args.paths), mods, args.trigger)
        selected = restrict_mods(selected, args.only, mods)
    except SelectionError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    publish_all = selected is None
    published = mods if publish_all else selected
    print(
        json.dumps(
            {
                "all": publish_all,
                "mods": published,
            }
        )
    )
    if args.github_output is not None:
        try:
            with args.github_output.open("a", encoding="utf-8", newline="\n") as handle:
                handle.write(f"publish_all={'true' if publish_all else 'false'}\n")
                handle.write(f"mods={' '.join(published)}\n")
                handle.write(f"count={len(published)}\n")
                if args.matrix:
                    handle.write(f"matrix={json.dumps(published or [MATRIX_PLACEHOLDER])}\n")
        except OSError as error:
            print(f"error: cannot write {args.github_output}: {error}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
