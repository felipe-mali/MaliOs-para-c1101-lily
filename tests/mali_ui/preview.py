"""Geometry/font preview for review; this is not a device screenshot or hardware test."""
from pathlib import Path
from PIL import Image, ImageDraw
import re,math
source=Path('src/core/ui/MaliTheme.h').read_text()
colors={name:'#'+value for name,value in re.findall(r'(\w+)=rgb\(0x([0-9a-f]+)\)',source)}
font=[int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',Path('lib/TFT_eSPI/Fonts/glcdfont.c').read_text())]
def preview(w,h,labels,index,name):
 im=Image.new('RGB',(w,h),colors['BACKGROUND']);d=ImageDraw.Draw(im)
 def line(a,b,c):d.line([a,b],fill=colors[c])
 def text(s,x,y,scale=1,c='TEXT_PRIMARY'):
  for ch in s:
   for col,byte in enumerate(font[ord(ch)*5:ord(ch)*5+5]):
    for row in range(8):
     if byte&(1<<row):d.rectangle((x+col*scale,y+row*scale,x+(col+1)*scale-1,y+(row+1)*scale-1),fill=colors[c])
   x+=6*scale
 def mark(x,y,r=11):
  for a,b in [((x-r,y+r),(x-r,y-r)),((x-r,y-r),(x,y+r//2)),((x,y+r//2),(x+r,y-r)),((x+r,y-r),(x+r,y+r)),((x-r+2,y+r),(x-r+2,y)),((x+r-2,y+r),(x+r-2,y))]:line(a,b,'ACCENT')
 def icon(label,x,y,size,c):
  r=size//2;a=r//2
  if label=='COUNTER':
   for i in range(4):
    ht=(2 if i==2 else i+2)*r//3;d.rounded_rectangle((x-r+i*r//2,y+r-ht,x-r+i*r//2+max(2,r//3)-1,y+r-1),radius=1,fill=colors[c])
  elif label in ['TOOLS','KEY GAUGE','PIXEL PAINT']:
   line((x-r,y+r),(x+r,y-r),c);line((x-r,y+r-2),(x+r-2,y-r),c);d.ellipse((x+r-5,y-r-1,x+r+1,y-r+5),outline=colors[c])
  else:d.ellipse((x-r,y-r,x+r,y+r),outline=colors[c])
 portrait=h>w;cy=26+(h-42)//2+(15 if portrait else 0);spacing=108 if portrait else 52
 gx=w//2 if portrait else 39;gy=cy-61 if portrait else cy;cardx=10 if portrait else 78;cardw=w-cardx-10
 details={'TOOLS':'Mali utilities','KEY GAUGE':'Profile viewer','COUNTER':'Resilience lab','RADIO':'RF / IR / NFC'}
 for k in range(-2,3):
  label=labels[(index+k)%len(labels)];selected=k==0;inset=0 if selected else 9;hh=(70 if selected else 52) if portrait else (55 if selected else 39);y=cy+k*spacing-hh//2
  d.rounded_rectangle((cardx+inset,y,cardx+cardw-inset-1,y+hh-1),radius=10,fill=colors['SURFACE' if selected else 'SURFACE_ALT'],outline=colors['ACCENT' if selected else 'BORDER'])
  icon(label,cardx+inset+19,y+hh//2,22 if selected else 14,'ACCENT' if selected else 'TEXT_DISABLED')
  size=2 if selected and len(label)*12<cardw-55 else 1;text(label,cardx+inset+37,y+hh//2-size*4-(6 if selected else 0),size,'TEXT_PRIMARY' if selected else 'TEXT_SECONDARY')
  if selected:text(details.get(label,'Click to open'),cardx+inset+37,y+hh//2+9,1,'TEXT_SECONDARY')
 r=23 if portrait else 27;d.ellipse((gx-r,gy-r,gx+r,gy+r),outline=colors['BORDER'])
 for i in range(12):
  a=i*math.pi/6;r=27 if portrait else 31;line((gx+int(math.cos(a)*r),gy+int(math.sin(a)*r)),(gx+int(math.cos(a)*(r+3)),gy+int(math.sin(a)*(r+3))),'ACCENT' if i<3 else 'ACCENT_DIM')
 mark(gx,gy)
 d.rectangle((0,0,w,25),fill=colors['BACKGROUND']);text('MALI / '+name,8,8);text('72%',w-26,8,1,'TEXT_SECONDARY');line((8,24),(w-8,24),'BORDER')
 d.rectangle((0,h-16,w,h),fill=colors['BACKGROUND']);text('Turn / Click / Hold: back' if portrait else 'Turn: select   Click: open   Hold: back',6,h-12,1,'TEXT_SECONDARY')
 im.save(f'tests/mali_ui/gear-{name.lower()}-{w}x{h}.png')
 return im
root=['NETWORK','RADIO','TOOLS','COUNTER','FILES','SYSTEM'];tools=['PIXEL PAINT','D20','COUNTER','KEY GAUGE','UTILITIES']
images=[preview(320,170,root,2,'OS'),preview(170,320,root,2,'OS'),preview(170,320,tools,3,'TOOLS')]
canvas=Image.new('RGB',(720,390),'#08090c');canvas.paste(images[0],(10,40));canvas.paste(images[1],(350,40));canvas.paste(images[2],(540,40));ImageDraw.Draw(canvas).text((10,10),'Geometry + TFT font preview / not a hardware capture',fill='#9c939f');canvas.resize((1440,780),Image.Resampling.NEAREST).save('tests/mali_ui/gear-review.png')
print('Rendered landscape + portrait geometry with the repository TFT bitmap font')
