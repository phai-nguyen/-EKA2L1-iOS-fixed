#!/usr/bin/env python3
"""Apply a unified diff by exact old-context matching, ignoring hunk line numbers.

This is intentionally stricter than fuzzy patching: each hunk's complete old text
(context + removed lines) must appear in the target file. Hunk line numbers are used
only to disambiguate multiple exact matches. Files are normalized to LF with a final
newline so patches can safely follow earlier diffs that left EOF without '\n'.
"""
from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path

HUNK_RE = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")
DIFF_RE = re.compile(r"^diff --git a/(.+) b/(.+)$")

@dataclass
class Hunk:
    old_start: int
    old_lines: list[str]
    new_lines: list[str]
    header: str


def parse_patch(text: str):
    lines = text.splitlines(keepends=True)
    files: list[tuple[str, list[Hunk]]] = []
    current_path: str | None = None
    hunks: list[Hunk] = []
    i = 0
    while i < len(lines):
        line = lines[i]
        m = DIFF_RE.match(line.rstrip("\n"))
        if m:
            if current_path is not None:
                files.append((current_path, hunks))
            current_path = m.group(2)
            hunks = []
            i += 1
            continue
        hm = HUNK_RE.match(line)
        if hm:
            if current_path is None:
                raise RuntimeError(f"Hunk outside diff: {line.rstrip()}")
            old_start = int(hm.group(1))
            header = line.rstrip("\n")
            old_lines: list[str] = []
            new_lines: list[str] = []
            i += 1
            while i < len(lines):
                cur = lines[i]
                if cur.startswith("diff --git ") or cur.startswith("@@ "):
                    break
                if cur.startswith("\\ No newline at end of file"):
                    i += 1
                    continue
                if not cur:
                    i += 1
                    continue
                prefix = cur[0]
                payload = cur[1:]
                if prefix == " ":
                    old_lines.append(payload)
                    new_lines.append(payload)
                elif prefix == "-":
                    old_lines.append(payload)
                elif prefix == "+":
                    new_lines.append(payload)
                elif cur.startswith(("--- ", "+++ ", "index ")):
                    pass
                else:
                    # Metadata outside the hunk body; do not silently consume source text.
                    raise RuntimeError(f"Unexpected patch line in {current_path}: {cur!r}")
                i += 1
            hunks.append(Hunk(old_start, old_lines, new_lines, header))
            continue
        i += 1
    if current_path is not None:
        files.append((current_path, hunks))
    return [(p, hs) for p, hs in files if hs]


def normalize_text(raw: bytes) -> str:
    text = raw.decode("utf-8")
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    if text and not text.endswith("\n"):
        text += "\n"
    return text


def all_occurrences(haystack: str, needle: str) -> list[int]:
    out: list[int] = []
    start = 0
    while True:
        pos = haystack.find(needle, start)
        if pos < 0:
            break
        out.append(pos)
        start = pos + 1
    return out


def line_number_at(text: str, byte_index: int) -> int:
    return text.count("\n", 0, byte_index) + 1


def apply_file(root: Path, rel: str, hunks: list[Hunk]) -> None:
    path = root / rel
    if not path.is_file():
        raise RuntimeError(f"Target file missing: {rel}")
    text = normalize_text(path.read_bytes())
    cumulative_delta = 0

    for index, h in enumerate(hunks, 1):
        old = "".join(h.old_lines)
        new = "".join(h.new_lines)
        if not old:
            raise RuntimeError(f"Unsupported zero-context insertion in {rel}: {h.header}")

        positions = all_occurrences(text, old)
        if not positions:
            preview = old[:600].replace("\n", "\\n\n")
            raise RuntimeError(
                f"Exact-context mismatch: {rel} hunk {index} {h.header}\n"
                f"Expected old block was not found. Preview:\n{preview}"
            )

        expected_line = max(1, h.old_start + cumulative_delta)
        if len(positions) == 1:
            pos = positions[0]
        else:
            pos = min(positions, key=lambda p: abs(line_number_at(text, p) - expected_line))
            nearest = line_number_at(text, pos)
            print(
                f"[exact-patch] {rel}: hunk {index} had {len(positions)} exact matches; "
                f"selected line {nearest} nearest expected {expected_line}"
            )

        actual_line = line_number_at(text, pos)
        text = text[:pos] + new + text[pos + len(old):]
        cumulative_delta += new.count("\n") - old.count("\n")
        print(f"[exact-patch] {rel}: hunk {index}/{len(hunks)} applied at line {actual_line} ({h.header})")

    path.write_bytes(text.encode("utf-8"))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True, type=Path)
    ap.add_argument("--patch", required=True, type=Path)
    args = ap.parse_args()

    parsed = parse_patch(args.patch.read_text(encoding="utf-8"))
    if not parsed:
        raise RuntimeError("No hunks found in patch")

    print(f"[exact-patch] applying {args.patch} to {args.root} ({len(parsed)} files)")
    for rel, hunks in parsed:
        apply_file(args.root, rel, hunks)
    print("[exact-patch] all hunks applied by exact context")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
