#!/usr/bin/env python3
"""
Auto-generate a Master Index (INDEX.md) for your markdown notes.

Usage:
    python indexer.py [root_directory]

If no root_directory is given, scans from the current working directory.
If no .md files are found, it prompts you to enter a different directory.
The generated INDEX.md is always written to the current directory.
"""

import os
import sys
from urllib.parse import quote

# ── Configuration ──────────────────────────────────────────────────────────
EXCLUDED_DIRS = {
    '.git', '__pycache__', '.venv', 'venv', 'node_modules',
    '.obsidian', '.trash', '.idea', '.vscode',
}
OUTPUT_FILE = 'INDEX.md'                 # name of the generated file
METADATA_ENABLED = True                  # set to False to skip front‑matter parsing

# Metadata tags -> emoji mapping (customize as you like)
METADATA_TAGS = {
    'status': {
        'draft':    '📝',
        'review':   '🔍',
        'complete': '✅',
    },
    'priority': {
        'high':   '🔴',
        'medium': '🟠',
        'low':    '🟢',
    },
}


def extract_metadata(filepath: str) -> str:
    """
    Look at the first 10 lines of a markdown file, search for lines like
    'Status: review' or 'Priority: high', and return a short emoji string
    to show in the index. Returns an empty string if nothing is found.
    """
    emojis = []
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            header = ''.join(f.readline() for _ in range(10))
    except (OSError, UnicodeDecodeError):
        return ''

    for line in header.splitlines():
        for key, mapping in METADATA_TAGS.items():
            if line.lower().startswith(f'{key}:'):
                value = line.split(':', 1)[1].strip().lower()
                if value in mapping:
                    emojis.append(mapping[value])

    return ' '.join(emojis)


def count_md_files(root_dir: str) -> int:
    """
    Walk a directory and count all .md files (excluding INDEX.md itself).
    Returns the total count.
    """
    output_path = os.path.join(os.getcwd(), OUTPUT_FILE)
    total = 0

    for dirpath, dirnames, filenames in os.walk(root_dir):
        dirnames[:] = [
            d for d in dirnames
            if d not in EXCLUDED_DIRS and not d.startswith('.')
        ]

        for f in filenames:
            if not f.lower().endswith('.md'):
                continue
            full_path = os.path.join(dirpath, f)
            if os.path.abspath(full_path) == output_path:
                continue
            total += 1

    return total


def prompt_for_directory() -> str:
    """
    Keep asking the user for a directory until they provide a valid one
    or choose to quit.
    """
    while True:
        user_input = input('\n📂 Enter a directory to scan (or "q" to quit): ').strip()

        if user_input.lower() == 'q':
            print('❌ Operation cancelled.')
            sys.exit(0)

        if user_input == '':
            continue

        # Expand user paths like ~/Documents
        expanded = os.path.expanduser(user_input)
        abs_path = os.path.abspath(expanded)

        if not os.path.exists(abs_path):
            print(f'⚠️  Path does not exist: {abs_path}')
            continue

        if not os.path.isdir(abs_path):
            print(f'⚠️  Not a directory: {abs_path}')
            continue

        return abs_path


def generate_index(root_dir: str = '.', output_file: str = OUTPUT_FILE) -> None:
    """
    Walk the directory tree, filter markdown files, and write a clean
    Master Index with relative links.
    """
    root_dir = os.path.abspath(root_dir)
    output_path = os.path.join(os.getcwd(), output_file)

    # Use a dict: key = relative folder path, value = list of (filename, full_path)
    folder_map = {}

    for dirpath, dirnames, filenames in os.walk(root_dir):
        # Prune excluded directories
        dirnames[:] = [
            d for d in dirnames
            if d not in EXCLUDED_DIRS and not d.startswith('.')
        ]

        # Filter markdown files, omit the output INDEX.md if it's in the root
        md_files = []
        for f in filenames:
            if not f.lower().endswith('.md'):
                continue
            full_path = os.path.join(dirpath, f)
            if os.path.abspath(full_path) == output_path:
                continue   # don't index the index itself
            md_files.append((f, full_path))

        if not md_files:
            continue

        # Relative folder path (use '.' for the root itself)
        rel_dir = os.path.relpath(dirpath, root_dir)
        folder_map.setdefault(rel_dir, []).extend(md_files)

    # ── Write the Markdown ─────────────────────────────────────────────
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('# Master Project Index\n\n')

        # Sort folders for a consistent output
        for folder, files in sorted(folder_map.items()):
            # Folder heading – replace path separators for readability
            heading = folder.replace(os.sep, ' › ') if folder != '.' else 'Root'
            f.write(f'## {heading}\n')

            # Sort files alphabetically
            for filename, full_path in sorted(files, key=lambda x: x[0].lower()):
                # Build relative link from the current working directory
                link_rel = os.path.relpath(full_path, os.getcwd())
                # Ensure forward slashes and encode spaces / special chars
                link_url = quote(link_rel.replace(os.sep, '/'))

                # Optional metadata emoji
                metadata = ''
                if METADATA_ENABLED:
                    metadata = extract_metadata(full_path)
                    if metadata:
                        metadata = f' {metadata}'

                f.write(f'* [{filename}]({link_url}){metadata}\n')
            f.write('\n')

    print(f'✅ Index created at {output_path}')


if __name__ == '__main__':
    # Allow optional root directory as first argument
    root = sys.argv[1] if len(sys.argv) > 1 else '.'

    # Keep asking the user for a directory until .md files are found
    while True:
        md_count = count_md_files(root)

        if md_count > 0:
            print(f'🔍 Found {md_count} markdown file(s) in {os.path.abspath(root)}')
            generate_index(root)
            break
        else:
            print(f'⚠️  No markdown files found in {os.path.abspath(root)}')
            root = prompt_for_directory()