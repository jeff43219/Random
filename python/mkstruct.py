#!/usr/bin/env python3
r"""
mkstruct.py - Build a folder/file structure from a tree-formatted text.

Usage:
    mkstruct.py                              # prompts for everything
    mkstruct.py structure.txt                # file input, prompts for output dir
    mkstruct.py structure.txt "D:\Projects"  # file input, explicit output dir
"""

import re
import sys
import argparse
from pathlib import Path

# ── Tree character stripping ──────────────────────────────────────────────────
_TREE_CHARS = re.compile(r'^[\s│├└─|`\-+\\]+')
_TREE_CONNECTORS = ("├──", "└──")
_HEADER_STRUCTURE_OF = re.compile(r'^\s*Structure\s+of\s*:', re.IGNORECASE)
_HEADER_SEPARATOR = re.compile(r'^\s*[=]{3,}\s*$')


def is_tree_entry_line(line: str) -> bool:
    s = line.rstrip("\r\n")
    if not s.strip():
        return False
    if _HEADER_STRUCTURE_OF.match(s) or _HEADER_SEPARATOR.match(s):
        return False
    return any(c in s for c in _TREE_CONNECTORS)


def resolve_input(raw: str) -> str:
    """
    If raw points to an existing file -> read it.
    Otherwise -> treat raw itself as tree text.
    """
    candidate = Path(raw.strip().strip('"').strip("'"))
    if candidate.exists() and candidate.is_file():
        try:
            return candidate.read_text(encoding='utf-8')
        except UnicodeDecodeError:
            return candidate.read_text(encoding='cp1252', errors='replace')
    return raw


def parse_line(line: str) -> tuple[int, str] | None:
    line = line.rstrip('\r\n')
    if not line.strip():
        return None
    match = _TREE_CHARS.match(line)
    prefix_len = match.end() if match else 0
    depth = prefix_len // 4
    name = line[prefix_len:].strip()
    if not name or name.startswith('#'):
        return None
    name = name.rstrip('/')
    if not name:
        return None
    return depth, name


def is_folder(name: str) -> bool:
    return '.' not in name


_WIN_INVALID_CHARS = set('<>:"/\\|?*')
_WIN_RESERVED_NAMES = {
    "CON", "PRN", "AUX", "NUL",
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
}


def validate_windows_segment(name: str) -> tuple[bool, str]:
    if not name:
        return False, "empty name"
    if any(ord(ch) < 32 for ch in name):
        return False, "contains control characters"
    if any(ch in _WIN_INVALID_CHARS for ch in name):
        bad = sorted({ch for ch in name if ch in _WIN_INVALID_CHARS})
        return False, f"contains invalid character(s): {''.join(bad)}"
    if name.endswith(" ") or name.endswith("."):
        return False, "ends with space or dot"
    base = name.split(".")[0].strip()
    if base.upper() in _WIN_RESERVED_NAMES:
        return False, f"reserved device name: {base.upper()}"
    return True, ""


def parse_structure(text: str) -> list[tuple[int, str, bool, int, str]]:
    entries: list[tuple[int, str, bool, int, str]] = []
    for line_no, line in enumerate(text.splitlines(), start=1):
        if not is_tree_entry_line(line):
            continue
        forced_folder = line.rstrip('\r\n ').endswith('/')
        result = parse_line(line)
        if result is None:
            continue
        depth, name = result
        folder = forced_folder or is_folder(name)
        entries.append((depth, name, folder, line_no, line.rstrip("\r\n")))
    return entries


def build_structure(entries: list[tuple[int, str, bool, int, str]], output_dir: Path):
    if not entries:
        print("No entries found. Nothing to build.")
        return

    stack: dict[int, Path] = {-1: output_dir}
    created_dirs = created_files = skipped = 0

    for depth, name, folder, line_no, src_line in entries:
        parent = stack.get(depth - 1)
        if parent is None:
            for d in range(depth - 1, -2, -1):
                if d in stack:
                    parent = stack[d]
                    break
        if parent is None:
            parent = output_dir

        target = parent / name

        valid, reason = validate_windows_segment(name)
        if not valid and sys.platform.startswith("win"):
            print(f"  [SKIP]   invalid name on line {line_no}: {name!r} ({reason})")
            print(f"           source: {src_line}")
            skipped += 1
            continue

        if folder:
            if target.exists():
                print(f"  [EXISTS]  {target}")
                skipped += 1
            else:
                target.mkdir(parents=True, exist_ok=True)
                print(f"  [DIR]     {target}")
                created_dirs += 1
            stack[depth] = target
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                print(f"  [EXISTS]  {target}")
                skipped += 1
            else:
                target.touch()
                print(f"  [FILE]    {target}")
                created_files += 1

    print()
    print(f"Done. {created_dirs} dirs, {created_files} files created — {skipped} skipped.")


def prompt(label: str, default: str = '') -> str:
    suffix = f" [{default}]" if default else ""
    return input(f"{label}{suffix}: ").strip().strip('"').strip("'")


def resolve_output_dir(raw: str) -> Path:
    p = Path(raw).resolve() if raw else Path.cwd()
    try:
        p.mkdir(parents=True, exist_ok=True)
    except PermissionError as e:
        print(f"ERROR: Cannot create/write to output directory: {p}", file=sys.stderr)
        if sys.platform.startswith("win"):
            suggested = Path.home() / "Desktop"
            print(f"Tip: On Windows your Desktop is usually: {suggested}", file=sys.stderr)
            print("Tip: Press Enter at the prompt to use the current directory.", file=sys.stderr)
        raise SystemExit(1) from e
    return p


def main():
    parser = argparse.ArgumentParser(
        description='Build a folder structure from tree-formatted text or a .txt file.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('input', nargs='?',
        help='Path to a .txt file OR raw tree text (omit to be prompted)')
    parser.add_argument('output_dir', nargs='?', default=None,
        help='Directory to build into (omit to be prompted)')
    args = parser.parse_args()

    is_tty = sys.stdin.isatty()

    # ── Resolve tree text ─────────────────────────────────────────────────────
    if args.input:
        text = resolve_input(args.input)
    else:
        if not is_tty:
            print("ERROR: No input provided and stdin is not a terminal.", file=sys.stderr)
            sys.exit(1)
        raw = prompt("Path to structure file (or paste tree text)")
        if not raw:
            print("ERROR: No input.", file=sys.stderr)
            sys.exit(1)
        text = resolve_input(raw)

    entries = parse_structure(text)
    if not entries:
        print("ERROR: No valid entries parsed.", file=sys.stderr)
        sys.exit(1)

    # ── Output directory — ALWAYS prompt if not given as arg ─────────────────
    if args.output_dir:
        output_dir = resolve_output_dir(args.output_dir)
    else:
        if not is_tty:
            output_dir = Path.cwd()
        else:
            raw_out = prompt("Output directory (Enter for cwd)", default=str(Path.cwd()))
            output_dir = resolve_output_dir(raw_out)

    # ── Build ─────────────────────────────────────────────────────────────────
    print(f"\nBuilding {len(entries)} entries into: {output_dir}\n")
    build_structure(entries, output_dir)


if __name__ == '__main__':
    main()