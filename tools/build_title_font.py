"""Generate the title's padded HAMMRF glyph atlas and font metrics."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument('font', type=Path, help='Original HAMMRF.TTF')
args = parser.parse_args()
bundle = Path(__file__).resolve().parents[1] / 'modules/live_widgets/assets/title'
document = bundle / 'Minotaur-Title.widgetml'
value = json.loads(document.read_text(encoding='utf-8'))
em, padding, columns, first, count = 48, 4, 16, 32, 96
font = ImageFont.truetype(str(args.font), em)
glyphs = [chr(c) for c in range(first, first + count)]
bounds = [font.getbbox(c, anchor='ls') for c in glyphs]
left = min(0, min(b[0] for b in bounds))
top = min(b[1] for b in bounds)
width = max(b[2] for b in bounds) - left + padding * 2
height = max(b[3] for b in bounds) - top + padding * 2
image = Image.new('RGBA', (width * columns, height * ((count + columns - 1) // columns)), (255, 255, 255, 0))
draw = ImageDraw.Draw(image)
advances = [font.getlength(c) for c in glyphs]
kerning = {}
for index, char in enumerate(glyphs):
    draw.text((index % columns * width + padding - left, index // columns * height + padding - top),
              char, font=font, anchor='ls', fill=(255, 255, 255, 255))
    for other, following in enumerate(glyphs):
        kern = font.getlength(char + following) - advances[index] - advances[other]
        if kern:
            kerning[f'{index},{other}'] = kern
image.save(bundle / 'assets/hammrf-raster.png')
value['fonts']['legal-raster'] = {'type': 'raster', 'atlas': 'legal-atlas', 'cell': [width, height],
    'columns': columns, 'first': first, 'count': count, 'em': em,
    'offset': [padding - left, padding], 'advances': advances, 'kerning': kerning,
    'lineHeight': height - padding * 2 + 2}
for item in value['root']['children']:
    if item['id'] == 'legal':
        for child in item['children']:
            if child['id'] == 'wording':
                child['fontSize'] = 36
document.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
