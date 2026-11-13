#!/usr/bin/env python3
"""Embed the recorded presentation payloads without changing their bytes."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--refresh", action="store_true",
                        help="Record hashes after intentionally replacing baked resources")
    args = parser.parse_args()
    if not args.refresh and args.output is None:
        parser.error("--output is required when embedding resources")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    if manifest["format"] != 1:
        raise ValueError("Unsupported baked-resource manifest")
    root = args.manifest.parent.resolve()
    total = 0
    for entry in manifest["assets"]:
        source = (root / entry["file"]).resolve()
        if not source.is_relative_to(root):
            raise ValueError("Resource path leaves the baked-resource directory")
        data = source.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if args.refresh:
            entry.update(bytes=len(data), sha256=digest)
            continue
        if len(data) != entry["bytes"] or digest != entry["sha256"]:
            raise ValueError(f"Changed resource: {entry['file']}; refresh its manifest intentionally")
        destination = args.output / Path(entry["file"]).with_suffix(".inc")
        text = "// Generated from the recorded baked resource.\n" + "".join(
            ",".join(str(value) for value in data[start:start + 24]) + ",\n"
            for start in range(0, len(data), 24)
        )
        if not destination.exists() or destination.read_text(encoding="ascii") != text:
            destination.parent.mkdir(parents=True, exist_ok=True)
            temporary = destination.with_suffix(".tmp")
            temporary.write_text(text, encoding="ascii", newline="\n")
            temporary.replace(destination)
        total += len(data)
    if args.refresh:
        args.manifest.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    else:
        print(f"Embedded {len(manifest['assets'])} verified baked resources ({total:,} bytes)")


if __name__ == "__main__":
    main()
