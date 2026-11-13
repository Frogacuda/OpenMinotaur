"""Make the deterministic runtime archive; desktop builds embed it without C byte arrays."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--archive', type=Path, required=True)
parser.add_argument('--embed', type=Path)
parser.add_argument('--format', choices=('windows', 'elf', 'apple'))
parser.add_argument('--game', help='Bundle one game directory instead of the collection')
args = parser.parse_args()
manifest = json.loads((args.source.parent / 'GAME-ASSETS.json').read_text(encoding='utf-8'))
files = sorted(f for f in args.source.rglob('*') if f.is_file())
expected=set(manifest['files'])
if args.game:
    assert args.game in {n.split('/')[0] for n in expected}, 'Unknown asset game'
    expected={n for n in expected if n.startswith(args.game+'/')}
    files=[f for f in files if f.relative_to(args.source).parts[0]==args.game]
assert {f.relative_to(args.source).as_posix() for f in files} == expected, 'Asset inventory differs from manifest'
args.archive.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for file in files:
        name = file.relative_to(args.source).as_posix()
        data = file.read_bytes()
        assert hashlib.sha256(data).hexdigest() == manifest['files'][name], f'Changed source asset: {name}'
        info = zipfile.ZipInfo(name, date_time=(2000, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        archive.writestr(info, data)
if args.embed:
    path = args.archive.resolve().as_posix()
    if args.format == 'windows':
        text = f'148 RCDATA "{path}"\n'
    else:
        apple = args.format == 'apple'
        section = '.section __TEXT,__const' if apple else '.section .rodata'
        prefix = '_' if apple else ''
        # The assembler includes bytes directly; no giant generated C++ arrays.
        lines = [section, '.balign 16', f'.globl {prefix}mino_game_assets_begin',
                 f'{prefix}mino_game_assets_begin:', f'.incbin "{path}"',
                 f'.globl {prefix}mino_game_assets_end', f'{prefix}mino_game_assets_end:']
        text = '// Generated resource embedding.\nasm(\n' + ''.join(json.dumps(line + '\n') + '\n' for line in lines) + ');\n'
    args.embed.write_text(text, encoding='utf-8')
print(f'Packed {len(files)} resources ({args.archive.stat().st_size} bytes)')
