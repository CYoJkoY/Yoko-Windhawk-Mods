#!/usr/bin/env python3
"""Amalgamate modular Windhawk mods into self-contained ``.wh.cpp`` files.

Each mod lives in ``mods/<mod-id>/src/`` and starts at ``src/main.cpp``. The
bundler inlines every local ``#include "..."`` relative to the including file,
deduplicates headers, strips include guards and ``#pragma once``, and keeps
angle-bracket system and Windhawk includes untouched. The result is written to
``dist/<mod-id>.wh.cpp`` with LF endings and a single final newline.

Usage:
    python tools/bundle.py                   # build every mod
    python tools/bundle.py --check           # fail if dist/ is stale
    python tools/bundle.py --mod cursor-tail
    python tools/bundle.py --json
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


class BundleError(Exception):
    """Raised when a mod cannot be bundled safely."""


MOD_ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")
LOCAL_INCLUDE_RE = re.compile(
    r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"[ \t]*(?://.*)?$'
)
PRAGMA_ONCE_RE = re.compile(r"^[ \t]*#[ \t]*pragma[ \t]+once[ \t]*(?://.*)?$")
GUARD_IFNDEF_RE = re.compile(
    r"^[ \t]*#[ \t]*ifndef[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*(?://.*)?$"
)
GUARD_DEFINE_RE = re.compile(
    r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*(?://.*)?$"
)
GUARD_ENDIF_RE = re.compile(r"^[ \t]*#[ \t]*endif\b[ \t]*(?://.*)?$")
METADATA_START = "// ==WindhawkMod=="
METADATA_END = "// ==/WindhawkMod=="
METADATA_ID_RE = re.compile(r"^//\s*@id\s+(\S+)\s*$")
METADATA_VERSION_RE = re.compile(r"^//\s*@version\s+(\S+)\s*$")
SUPPORTED_SUFFIXES = {".h", ".hpp", ".hh", ".inc", ".cpp"}


@dataclass(frozen=True)
class BundleStats:
    """Deterministic description of one amalgamated mod."""

    mod_id: str
    entry: str
    output: str
    files: tuple[str, ...]
    source_lines: int
    output_lines: int
    changed: bool

    @property
    def ratio(self) -> float:
        if self.source_lines == 0:
            return 0.0
        return self.output_lines / self.source_lines

    def as_dict(self) -> dict[str, object]:
        return {
            "mod_id": self.mod_id,
            "entry": self.entry,
            "output": self.output,
            "files": list(self.files),
            "source_lines": self.source_lines,
            "output_lines": self.output_lines,
            "ratio": round(self.ratio, 3),
            "changed": self.changed,
        }


def _strip_block_comments(line: str, in_block: bool) -> tuple[str, bool]:
    """Remove block comments from one line while tracking comment state."""

    kept: list[str] = []
    index = 0
    while index < len(line):
        if in_block:
            end = line.find("*/", index)
            if end == -1:
                return "".join(kept), True
            in_block = False
            index = end + 2
            continue
        start = line.find("/*", index)
        if start == -1:
            kept.append(line[index:])
            break
        kept.append(line[index:start])
        in_block = True
        index = start + 2
    return "".join(kept), in_block


def _code_lines(lines: list[str]) -> list[tuple[int, str, bool]]:
    """Yield ``(index, code, in_block_comment)`` for every source line."""

    result: list[tuple[int, str, bool]] = []
    in_block = False
    for index, line in enumerate(lines):
        code, in_block = _strip_block_comments(line, in_block)
        result.append((index, code, in_block))
    return result


def _significant_indexes(lines: list[str]) -> list[int]:
    """Return indexes of lines holding anything besides whitespace/comments."""

    result: list[int] = []
    for index, code, _ in _code_lines(lines):
        stripped = code.strip()
        if not stripped or stripped.startswith("//"):
            continue
        result.append(index)
    return result


def _strip_include_guard(text: str) -> str:
    """Remove every ``#pragma once`` and a single matching include guard."""

    lines = text.split("\n")
    code_lines = _code_lines(lines)
    remove: set[int] = set()

    for index, code, _ in code_lines:
        if PRAGMA_ONCE_RE.match(code):
            remove.add(index)

    significant = [index for index in _significant_indexes(lines) if index not in remove]
    if len(significant) >= 3:
        first, second, last = significant[0], significant[1], significant[-1]
        ifndef = GUARD_IFNDEF_RE.match(lines[first])
        define = GUARD_DEFINE_RE.match(lines[second])
        endif = GUARD_ENDIF_RE.match(lines[last])
        if ifndef and define and endif and ifndef.group(1) == define.group(1):
            remove.update((first, second, last))

    if not remove:
        return text
    kept = [line for index, line in enumerate(lines) if index not in remove]
    return "\n".join(kept)


def _normalize_rel(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def _resolve_local_include(spec: str, including: Path, root: Path) -> Path:
    """Resolve a quoted include and enforce repository containment."""

    if not spec or spec.startswith(("/", "\\")):
        raise BundleError(f"absolute include is not allowed: {spec!r}")
    if re.match(r"^[A-Za-z]:", spec):
        raise BundleError(f"absolute include is not allowed (drive-qualified): {spec!r}")
    candidate = Path(spec.replace("\\", "/"))
    if candidate.is_absolute():
        raise BundleError(f"absolute include is not allowed: {spec!r}")
    resolved = (including.parent / candidate).resolve()
    try:
        resolved.relative_to(root)
    except ValueError as error:
        raise BundleError(
            f"include escapes the repository: {spec!r} from {including}"
        ) from error
    if resolved.suffix.lower() not in SUPPORTED_SUFFIXES:
        raise BundleError(f"unsupported include type: {spec!r} from {including}")
    if not resolved.is_file():
        raise BundleError(f"missing include: {spec!r} from {including}")
    return resolved


class Bundler:
    """Expand one translation unit from a mod entry source."""

    def __init__(self, root: Path) -> None:
        self.root = root.resolve()
        self._emitted: set[Path] = set()
        self._stack: list[Path] = []
        self._inlined: list[Path] = []

    def build(self, entry: Path) -> tuple[str, list[Path], int]:
        entry = entry.resolve()
        try:
            entry.relative_to(self.root)
        except ValueError as error:
            raise BundleError(f"entry is outside the repository: {entry}") from error
        if not entry.is_file():
            raise BundleError(f"missing entry source: {entry}")
        text = self._expand(entry)
        return text, list(self._inlined), self._count_source_lines(entry)

    def _read(self, path: Path) -> str:
        try:
            raw = path.read_bytes()
        except OSError as error:
            raise BundleError(f"cannot read {path}: {error}") from error
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError as error:
            raise BundleError(f"source is not UTF-8: {path}: {error}") from error
        if text.startswith("\ufeff"):
            text = text[1:]
        return text.replace("\r\n", "\n").replace("\r", "\n")

    def _expand(self, path: Path) -> str:
        resolved = path.resolve()
        if resolved in self._stack:
            chain = [_normalize_rel(item, self.root) for item in self._stack]
            chain.append(_normalize_rel(resolved, self.root))
            raise BundleError(f"include cycle: {' -> '.join(chain)}")
        if resolved in self._emitted:
            return ""

        self._stack.append(resolved)
        try:
            text = _strip_include_guard(self._read(resolved))
            self._emitted.add(resolved)
            self._inlined.append(resolved)
            output: list[str] = []
            in_block = False
            for line in text.split("\n"):
                code, in_block = _strip_block_comments(line, in_block)
                include = None if in_block else LOCAL_INCLUDE_RE.match(code)
                if include is None:
                    output.append(line)
                    continue
                target = _resolve_local_include(include.group(1), resolved, self.root)
                expanded = self._expand(target)
                if expanded:
                    output.append(expanded.rstrip("\n"))
            return "\n".join(output)
        finally:
            self._stack.pop()

    def _count_source_lines(self, entry: Path) -> int:
        """Count the unique pre-expansion lines behind one bundle."""

        return sum(len(_strip_include_guard(self._read(path)).split("\n")) for path in self._inlined)


def _read_manifest(mod_dir: Path) -> dict[str, object]:
    manifest_path = mod_dir / "mod.json"
    if not manifest_path.is_file():
        raise BundleError(f"missing mod manifest: {manifest_path}")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise BundleError(f"invalid mod manifest: {manifest_path}: {error}") from error
    if not isinstance(manifest, dict):
        raise BundleError(f"mod manifest must be an object: {manifest_path}")
    return manifest


def _validate_metadata(mod_id: str, text: str, manifest: dict[str, object]) -> None:
    """Ensure the bundle opens with the official metadata block."""

    lines = text.split("\n")
    if not lines or lines[0].strip() != METADATA_START:
        raise BundleError(
            f"{mod_id}: main.cpp must start with the metadata block, got {lines[0]!r}"
        )
    if METADATA_END not in lines:
        raise BundleError(f"{mod_id}: metadata block is missing {METADATA_END}")

    version = None
    metadata_id = None
    for line in lines[1 : lines.index(METADATA_END)]:
        stripped = line.strip()
        if metadata_id is None:
            id_match = METADATA_ID_RE.match(stripped)
            if id_match:
                metadata_id = id_match.group(1)
        if version is None:
            version_match = METADATA_VERSION_RE.match(stripped)
            if version_match:
                version = version_match.group(1)
        if metadata_id is not None and version is not None:
            break
    declared = manifest.get("version")
    if metadata_id != mod_id:
        raise BundleError(
            f"{mod_id}: metadata @id {metadata_id!r} does not match the directory"
        )
    if not isinstance(version, str) or not version:
        raise BundleError(f"{mod_id}: metadata block has no @version")
    if version != declared:
        raise BundleError(
            f"{mod_id}: metadata @version {version!r} != mod.json version {declared!r}"
        )
    manifest_id = manifest.get("id")
    if manifest_id != mod_id:
        raise BundleError(
            f"{mod_id}: mod.json id {manifest_id!r} does not match the directory"
        )


def _write_atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temp_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=str(path.parent)
    )
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_name, path)
    except BaseException:
        try:
            os.unlink(temp_name)
        except OSError:
            pass
        raise


def _discover(root: Path, mod_filter: str | None) -> list[Path]:
    mods_root = root / "mods"
    if not mods_root.is_dir():
        raise BundleError(f"missing mods directory: {mods_root}")
    if mod_filter is not None and not MOD_ID_RE.match(mod_filter):
        raise BundleError(f"invalid mod id: {mod_filter!r}")
    entries = sorted(mods_root.glob("*/src/main.cpp"))
    if mod_filter is not None:
        entries = [path for path in entries if path.parent.parent.name == mod_filter]
        if not entries:
            raise BundleError(f"mod not found: {mod_filter}")
    return entries


def bundle_all(
    root: Path,
    mod_filter: str | None = None,
    check: bool = False,
    output_root: Path | None = None,
) -> list[BundleStats]:
    root = root.resolve()
    dist_root = (output_root if output_root is not None else root / "dist").resolve()
    results: list[BundleStats] = []
    for entry in _discover(root, mod_filter):
        mod_dir = entry.parent.parent
        mod_id = mod_dir.name
        if not MOD_ID_RE.match(mod_id):
            raise BundleError(f"invalid mod directory name: {mod_id!r}")
        manifest = _read_manifest(mod_dir)
        text, inlined, source_lines = Bundler(root).build(entry)
        if not text.endswith("\n"):
            text += "\n"
        _validate_metadata(mod_id, text, manifest)
        output_path = dist_root / f"{mod_id}.wh.cpp"
        data = text.encode("utf-8")
        changed = False
        try:
            output_label = _normalize_rel(output_path, root)
        except ValueError:
            output_label = output_path.as_posix()
        if check:
            existing = output_path.read_bytes() if output_path.is_file() else None
            if existing != data:
                raise BundleError(
                    f"stale bundle: {output_label} (run python tools/bundle.py)"
                )
        else:
            previous = output_path.read_bytes() if output_path.is_file() else None
            changed = previous != data
            if changed:
                _write_atomic(output_path, data)
        results.append(
            BundleStats(
                mod_id=mod_id,
                entry=_normalize_rel(entry, root),
                output=output_label,
                files=tuple(_normalize_rel(path, root) for path in inlined),
                source_lines=source_lines,
                output_lines=text.count("\n"),
                changed=changed,
            )
        )
    return results


def _print_report(stats: BundleStats, as_json: bool) -> None:
    if as_json:
        print(json.dumps(stats.as_dict(), ensure_ascii=True, sort_keys=True))
        return
    action = "wrote" if stats.changed else "up to date"
    print(f"[{action}] {stats.mod_id}: {stats.entry} -> {stats.output}")
    print(
        f"  lines: {stats.source_lines} source -> {stats.output_lines} output "
        f"({stats.ratio:.2f}x)"
    )
    print(f"  inlined: {len(stats.files)} files")
    for name in stats.files:
        print(f"    - {name}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Amalgamate modular Windhawk mods into dist/<mod-id>.wh.cpp."
    )
    parser.add_argument("--root", type=Path, default=None, help="repository root")
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="bundle output directory (default: <root>/dist)",
    )
    parser.add_argument("--mod", dest="mod_id", default=None, help="bundle one mod")
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify dist/ matches the sources instead of writing",
    )
    parser.add_argument("--json", action="store_true", help="print one JSON object per mod")
    args = parser.parse_args(argv)

    root = args.root if args.root is not None else Path(__file__).resolve().parent.parent
    try:
        results = bundle_all(root, args.mod_id, args.check, args.output)
    except BundleError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    if not results:
        print("error: no mods found", file=sys.stderr)
        return 1
    for stats in results:
        _print_report(stats, args.json)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
