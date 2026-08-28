#!/usr/bin/env python3
"""Apply unified-diff hunks by strict code-line context.

Hunk line numbers are only hints. Non-blank source lines must match the patch's
old side exactly after removing line endings and trailing spaces/tabs. Blank-line
count may differ. This is deliberately not fuzzy matching: non-blank lines may not
be changed, reordered, or skipped.
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


def parse_patch(text: str) -> list[tuple[str, list[Hunk]]]:
    lines = text.splitlines(keepends=True)
    files: list[tuple[str, list[Hunk]]] = []
    current_path: str | None = None
    hunks: list[Hunk] = []
    i = 0

    while i < len(lines):
        line = lines[i]
        dm = DIFF_RE.match(line.rstrip("\r\n"))
        if dm:
            if current_path is not None:
                files.append((current_path, hunks))
            current_path = dm.group(2)
            hunks = []
            i += 1
            continue

        hm = HUNK_RE.match(line)
        if not hm:
            i += 1
            continue

        if current_path is None:
            raise RuntimeError(f"Hunk outside diff: {line.rstrip()}")

        old_start = int(hm.group(1))
        header = line.rstrip("\r\n")
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
                raise RuntimeError(f"Unexpected patch line in {current_path}: {cur!r}")
            i += 1

        hunks.append(Hunk(old_start, old_lines, new_lines, header))

    if current_path is not None:
        files.append((current_path, hunks))
    return [(path, hs) for path, hs in files if hs]


def normalize_file(raw: bytes) -> str:
    text = raw.decode("utf-8")
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    if text and not text.endswith("\n"):
        text += "\n"
    return text


def code_line(line: str) -> str:
    return line.rstrip("\r\n").rstrip(" \t")


def is_blank(line: str) -> bool:
    return code_line(line) == ""


def nonblank_old(hunk: Hunk) -> list[str]:
    return [code_line(line) for line in hunk.old_lines if not is_blank(line)]


def find_candidates(source_lines: list[str], wanted: list[str]) -> list[tuple[int, int]]:
    """Return [start,end) physical spans matching all wanted code lines in order.

    Only blank source lines may be skipped between wanted lines.
    """
    if not wanted:
        return []

    candidates: list[tuple[int, int]] = []
    first = wanted[0]
    for start in range(len(source_lines)):
        if code_line(source_lines[start]) != first:
            continue
        si = start
        ok = True
        for expected in wanted:
            while si < len(source_lines) and is_blank(source_lines[si]):
                si += 1
            if si >= len(source_lines) or code_line(source_lines[si]) != expected:
                ok = False
                break
            si += 1
        if ok:
            candidates.append((start, si))
    return candidates


def apply_file(root: Path, rel: str, hunks: list[Hunk]) -> None:
    path = root / rel
    if not path.is_file():
        raise RuntimeError(f"Target file missing: {rel}")

    text = normalize_file(path.read_bytes())
    cumulative_delta = 0

    for index, hunk in enumerate(hunks, 1):
        source_lines = text.splitlines(keepends=True)
        wanted = nonblank_old(hunk)
        if len(wanted) < 2:
            raise RuntimeError(
                f"Refusing weak context ({len(wanted)} non-blank lines): {rel} {hunk.header}"
            )

        candidates = find_candidates(source_lines, wanted)
        if not candidates:
            preview = "\n".join(wanted[:16])
            raise RuntimeError(
                f"Code-context mismatch: {rel} hunk {index}/{len(hunks)} {hunk.header}\n"
                "All non-blank code lines must match exactly; only blank-line count and "
                f"trailing whitespace may differ.\nExpected code preview:\n{preview}"
            )

        expected_line = max(1, hunk.old_start + cumulative_delta)
        start, end = min(candidates, key=lambda span: abs((span[0] + 1) - expected_line))
        if len(candidates) > 1:
            print(
                f"[context-patch] {rel}: hunk {index} has {len(candidates)} strict-code matches; "
                f"selected line {start + 1} nearest hint {expected_line}"
            )

        replacement = "".join(hunk.new_lines)
        before = "".join(source_lines[:start])
        after = "".join(source_lines[end:])
        text = before + replacement + after

        old_physical_count = end - start
        new_physical_count = replacement.count("\n")
        cumulative_delta += new_physical_count - old_physical_count
        print(
            f"[context-patch] {rel}: hunk {index}/{len(hunks)} applied at line {start + 1} "
            f"({hunk.header}) [strict-code / blank-whitespace tolerant]"
        )

    path.write_bytes(text.encode("utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--patch", required=True, type=Path)
    args = parser.parse_args()

    parsed = parse_patch(args.patch.read_text(encoding="utf-8"))
    if not parsed:
        raise RuntimeError("No hunks found in patch")

    print(f"[context-patch] applying {args.patch} to {args.root} ({len(parsed)} files)")
    for rel, hunks in parsed:
        apply_file(args.root, rel, hunks)
    print("[context-patch] all hunks applied with strict code-line context")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
