#!/usr/bin/env python3
"""Check or format project-owned C/C++ sources with the pinned clang-format."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

VERSION = "22.1.3"
ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = {".c", ".cpp", ".h", ".hpp", ".inl"}


def sources():
    for directory, children, files in os.walk(ROOT):
        children[:] = sorted(
            child for child in children
            if not child.startswith("build")
            and child not in {".git", ".cxx", ".gradle", "third_party", "assets", "_deps", ".venv", "__pycache__"}
        )
        for name in sorted(files):
            path = Path(directory) / name
            if path.suffix in EXTENSIONS:
                yield path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--fix", action="store_true")
    parser.add_argument("--clang-format", default="clang-format")
    args = parser.parse_args()
    version = subprocess.check_output([args.clang_format, "--version"], text=True)
    if not re.search(r"\bversion " + re.escape(VERSION) + r"\b", version):
        parser.error(f"clang-format {VERSION} is required; found {version.strip()}")
    changed = []
    count = 0
    for path in sources():
        count += 1
        original = path.read_bytes()
        formatted = subprocess.check_output(
            [args.clang_format, "--style=file", f"--assume-filename={path}"],
            input=original,
        )
        if formatted != original:
            changed.append(path.relative_to(ROOT).as_posix())
            if args.fix:
                path.write_bytes(formatted)
    print(f"{count} source files; {len(changed)} {'formatted' if args.fix else 'need formatting'}.")
    if args.check and changed:
        for name in changed[:20]:
            print(name)
        if len(changed) > 20:
            print(f"... and {len(changed) - 20} more")
        print("Run tools/format_sources.py --fix with the pinned formatter.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
