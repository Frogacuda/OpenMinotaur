"""Shared output contract for the optional presentation-resource bakers."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "assets/baked/MANIFEST.json"


def write_payload(consumer, symbol, data):
    """Update an existing baked payload and return its generated include name."""
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    entries = [entry for entry in manifest["assets"]
               if entry["consumer"] == consumer and entry["symbol"] == symbol]
    if len(entries) != 1:
        raise ValueError(f"Add one manifest entry for {consumer}:{symbol} before baking")
    entry = entries[0]
    destination = (MANIFEST.parent / entry["file"]).resolve()
    if not destination.is_relative_to(MANIFEST.parent.resolve()):
        raise ValueError("Baked-resource destination leaves the resource directory")
    destination.write_bytes(data)
    entry.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return "baked_assets/" + Path(entry["file"]).with_suffix(".inc").as_posix()
