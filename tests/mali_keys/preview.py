"""Geometry review using the TFT bitmap font; not a hardware screenshot.

Example dimensions below are arbitrary test data, not a manufacturer/key model.
"""
from pathlib import Path
from PIL import Image, ImageDraw
import re
ROOT=Path(__file__).resolve().parents[2]
colors={n:'#'+v for n,v in re.findall(r'(\w+)=rgb\(0x([0-9a-f]+)\)',(ROOT/'src/core/ui/MaliTheme.h').read_text())}
font=[int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',(ROOT/'lib/TFT_eSPI/Fonts/glcdfont.c').read_text())]
def render(w,h,cross=False,face=0):
    im=Image.new('RGB',(w,h),colors['BACKGROUND']);d=ImageDraw.Draw(im)
    def text(s,x,y,width,color='TEXT_SECONDARY'):
        n=max(0,width//6)
        if len(s)>n:s=s[:max(0,n-3)]+'...'
        for ch in s:
            for col,b in enumerate(font[ord(ch)*5:ord(ch)*5+5]):
                for row in range(8):
                    if b&(1<<row):d.point((x+col,y+row),fill=colors[color])
            x+=6
    def side(x,y,w,h):
        total,useful,width=6000,4000,800
        head=total-useful;headw=max(width*2,head*4//5)
        scale=min((w-8)/total,(h-12)/max(headw,width))
        span,hx,bh,hh=int(total*scale),max(2,int(head*scale)),max(2,int(width*scale)),max(2,int(headw*scale))
        cx=x+(w-span)//2;cy=y+h//2;shoulder=cx+hx;tip=cx+span-1;point=min(max(2,bh//2),max(2,tip-shoulder))
        steel=colors['TEXT_SECONDARY']
        d.ellipse((cx,cy-hh//2,cx+hx,cy+hh//2),fill=steel)
        if hx>10 and hh>10:d.ellipse((cx+hx//2-hx//6,cy-hh//6,cx+hx//2+hx//6,cy+hh//6),fill=colors['SURFACE'])
        d.rectangle((shoulder,cy-bh//2,tip-point,cy+(bh-1)//2),fill=steel)
        d.polygon([(tip-point,cy-bh//2),(tip,cy),(tip-point,cy+(bh-1)//2)],fill=steel)
        d.line((shoulder,cy-bh//2-2,shoulder,cy+(bh-1)//2+2),fill=colors['ACCENT'])
        for i in range(1 if cross else 2):
            gy=cy-bh//2+1+(bh-2)*(i+1)//(2 if cross else 3)
            d.line((shoulder+1,gy,tip-point,gy),fill=colors['SURFACE_ALT'])
        if not cross:
            for a in range(shoulder+3,tip-point-2,5):d.line((a,cy-bh//2+1,a+2,cy-bh//2+min(3,max(1,bh//3))),fill=colors['ACCENT_DIM'])
        body=max(1,tip-point-shoulder);markers=body*100//150
        for i in range(6):
            a=shoulder+(body-markers)//2+markers*i//5
            d.line((a,cy+bh//2+4,a,cy+bh//2+6),fill=colors['ACCENT'])
    def front(x,y,w,h):
        arms=[600,600,600,600];widths=[240]*4
        scale=min((w-32)/1200,(h-30)/1200)
        cx=x+w//2;cy=y+h//2
        for i in [a for a in range(4) if a!=face]+[face]:
            a,b=max(2,int(arms[i]*scale)),max(2,int(widths[i]*scale))
            boxes=[(cx-b//2,cy-a,b,a),(cx,cy-b//2,a,b),(cx-b//2,cy,b,a),(cx-a,cy-b//2,a,b)]
            rx,ry,rw,rh=boxes[i];d.rectangle((rx,ry,rx+rw-1,ry+rh-1),fill=colors['ACCENT' if i==face else 'TEXT_SECONDARY'])
        for i,(tx,ty) in enumerate([(cx-3,y+1),(x+w-10,cy-4),(cx-3,y+h-10),(x+2,cy-4)]):text(chr(65+i),tx,ty,8,'ACCENT' if i==face else 'TEXT_SECONDARY')
    text('MALI / '+('Chave Cruciforme' if cross else 'Chave Plana'),8,8,w-42,'TEXT_PRIMARY')
    d.line((8,24,w-8,24),fill=colors['BORDER'])
    x,y,cw,ch=8,32,w-16,h-58
    d.rounded_rectangle((x,y,x+cw-1,y+ch-1),radius=10,fill=colors['SURFACE'],outline=colors['ACCENT'])
    text('Referencia de teste',x+8,y+8,cw-16,'TEXT_PRIMARY')
    art=y+24;info=y+ch-38;infoWidth=cw-16
    if cross:
        text('Face '+chr(65+face)+' / Direita',x+8,art,cw-16,'ACCENT');art+=14;ah=info-art-3
        if h>w:
            side(x+4,art,cw-8,ah//3);front(x+8,art+ah//3,cw-16,ah*2//3)
        else:
            fw=min(76,cw//3);side(x+4,art,cw-fw-8,ah);front(x+cw-fw-4,y+24,fw,ch-32);infoWidth=cw-fw-16
    else:side(x+4,art,cw-8,info-art-4)
    text('C: 60,00 U: 40,00 mm',x+8,info,infoWidth)
    text('L: 8,00 E: 2,00 mm',x+8,info+12,infoWidth)
    text('Esquema ilustrativo',x+8,info+24,infoWidth,'ACCENT')
    text('Girar: face OK: opcoes' if cross else 'OK: opcoes Seg: voltar',6,h-12,w-12)
    im.save(ROOT/f'tests/mali_keys/{"cross" if cross else "flat"}-{w}x{h}-{face}.png')
    return im
images=[render(320,170),render(170,320),render(320,170,True,0),render(170,320,True,0)]
canvas=Image.new('RGB',(690,560),colors['BACKGROUND']);d=ImageDraw.Draw(canvas)
d.text((12,8),'Previa geometrica - dados ficticios - nao e captura do aparelho',fill=colors['TEXT_SECONDARY'])
canvas.paste(images[0],(12,40));canvas.paste(images[2],(12,228));canvas.paste(images[1],(350,40));canvas.paste(images[3],(530,40))
for i in range(1,4):render(170,320,True,i)
canvas.resize((1380,1120),Image.Resampling.NEAREST).save(ROOT/'tests/mali_keys/preview.png')
print('PASS: flat/cross geometry previews, both orientations and all four face highlights')
