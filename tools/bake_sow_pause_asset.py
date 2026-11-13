"""Compose pause plates from Super Ox Wars button artwork and lettering."""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageDraw,ImageChops

root=Path(__file__).resolve().parents[1]
assets=root/'assets/games/superoxwars'
atlas=Image.open(assets/'sf.png').convert('RGBA')
plate=atlas.crop((112,0,209,25))
ImageDraw.Draw(plate).rectangle((5,3,91,21),fill=atlas.getpixel((117,10)))
# White-ink bounds; retain the original two-pixel antialiased outline.
bounds={'T':(140,7,149,19),'I':(148,35,150,48),
        'L':(273,56,280,69),'E':(125,36,132,48),
        'O':(249,30,260,43),'R':(264,31,273,43),
        'S':(225,30,234,43),'C':(237,30,246,43)}
glyphs={}
for c,(x,y,r,b) in bounds.items():
    glyph=atlas.crop((x-2,y-2,r+2,b+2))
    ink=glyph.convert('L').point(lambda v:max(0,min(255,int((v-100)*255/155))))
    clean=Image.new('RGBA',glyph.size,atlas.getpixel((117,10)))
    outline=Image.new('L',ink.size)
    for dx in range(-2,3):
        for dy in range(-2,3):
            if dx*dx+dy*dy<=4:outline=ImageChops.lighter(outline,ImageChops.offset(ink,dx,dy))
    clean.paste((0,0,0,255),mask=outline)
    clean.paste((255,255,255,255),mask=ink);glyphs[c]=clean
width=sum(glyphs[c].width for c in 'TITLE')+8
x=(97-width)//2
for c in 'TITLE':
    glyph=glyphs[c];plate.paste(glyph,(x,21-glyph.height));x+=glyph.width+2
page=Image.new('RGBA',(512,512));page.paste(plate,(1,1))
# N and K are absent from the original menu labels. Construct their strokes
# at the same cap height, weight and two-pixel outline as the extracted ink.
for letter,strokes in {'N':[[(1,11),(1,1),(8,11),(8,1)]],
                       'K':[[(1,1),(1,11)],[(8,1),(1,6),(8,11)]]}.items():
    ink=Image.new('L',(52,64));draw=ImageDraw.Draw(ink)
    for points in strokes:
        points=[(int((x+2)*4),int((y+2)*4)) for x,y in points]
        draw.line(points,fill=255,width=8,joint='curve')
        for x,y in points:draw.ellipse((x-4,y-4,x+4,y+4),fill=255)
    ink=ink.resize((13,16),Image.Resampling.LANCZOS)
    clean=Image.new('RGBA',ink.size,atlas.getpixel((117,10)));outline=Image.new('L',ink.size)
    for dx in range(-2,3):
        for dy in range(-2,3):
            if dx*dx+dy*dy<=4:outline=ImageChops.lighter(outline,ImageChops.offset(ink,dx,dy))
    clean.paste((0,0,0,255),mask=outline);clean.paste((255,255,255,255),mask=ink);glyphs[letter]=clean
for top,text in [(28,'TILT ON'),(55,'R STICK')]:
    button=atlas.crop((112,0,209,25));ImageDraw.Draw(button).rectangle((5,3,91,21),fill=atlas.getpixel((117,10)))
    width=sum(6 if c==' ' else glyphs[c].width for c in text);x=(97-width)//2
    for c in text:
        if c==' ':x+=6;continue
        glyph=glyphs[c];button.paste(glyph,(x,21-glyph.height));x+=glyph.width
    page.paste(button,(1,top))
page.save(assets/'pause-buttons.png')
p=assets/'NeonResources.xml';s=p.read_text()
if '<Literal>pause-buttons.png</Literal>' not in s:
    s=s.replace("literals='2'","literals='3'").replace("spritepages='2'","spritepages='3'")
    s=s.replace("entries='93'","entries='94'")
    s=s.replace('</Literals>','<Literal>pause-buttons.png</Literal>\n  </Literals>')
    s=s.replace('</Spritemap>','<Spentry>2,1,1,97,25,</Spentry>\n   </Spritemap>')
    s=s.replace('</SpritePages>','<Spritepage>512,512,</Spritepage>\n  </SpritePages>')
    p.write_text(s,encoding='utf-8')
if '<Spentry>2,1,28,97,25,</Spentry>' not in s:
    s=s.replace("entries='94'","entries='96'")
    s=s.replace('</Spritemap>','<Spentry>2,1,28,97,25,</Spentry>\n<Spentry>2,1,55,97,25,</Spentry>\n</Spritemap>')
    p.write_text(s,encoding='utf-8')
p=root/'assets/GAME-ASSETS.json';data=json.loads(p.read_text())
for name in ['NeonResources.xml','pause-buttons.png']:
    data['files']['superoxwars/'+name]=hashlib.sha256((assets/name).read_bytes()).hexdigest()
p.write_text(json.dumps(data,indent=2)+'\n')
