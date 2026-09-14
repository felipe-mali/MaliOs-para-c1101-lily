#include "KeyRenderer.h"
#include "KeyMeasurement.h"
#include "core/ui/MaliUI.h"
#include "core/ui/KeysPtBr.h"
#include <algorithm>
namespace MaliKeys {
namespace T = MaliText::Keys;
namespace {
void text(int x, int y, const String &value, int width, uint16_t color = MaliUI::TEXT_SECONDARY) {
    tft.setTextSize(1); tft.setTextDatum(0); tft.setTextColor(color, MaliUI::SURFACE);
    tft.drawString(MaliUI::fitText(value, width), x, y, 1);
}
// Side view is a silhouette plus an annotated region, without a cut-depth array.
void sideView(const KeyProfile &p, int face, int x, int y, int w, int h, bool guides) {
    bool cross = p.type == KeyType::Cruciform;
    const auto &f = p.cross.faces[face];
    int total = p.length ? p.length : 6000;
    int useful = p.useful ? p.useful : total*2/3;
    int bodyWidth = cross && f.width ? f.width : p.width ? p.width : total/7;
    int headLength = max(1, total-useful);
    int headWidth = max(bodyWidth*2, headLength*4/5);
    float scale = std::min(float(w-8)/total, float(h-12)/max(1,max(headWidth,bodyWidth)));
    int span = max(5, int(total*scale)), hx = max(2,int(headLength*scale));
    int bh = max(2,int(bodyWidth*scale)), hh = max(bh,int(headWidth*scale));
    int cx = x+(w-span)/2, cy = y+h/2, shoulder = cx+hx, tip = cx+span-1;
    int point = min(max(2,bh/2), max(2,tip-shoulder));
    bool mirror = (p.orientation == Orientation::Left) != (cross && f.orientation == Orientation::Left);
    auto px = [&](int value) { return mirror ? x+w-1-(value-x) : value; };
    auto rect = [&](int a,int b,int rw,int rh,uint16_t color) {
        tft.fillRect(mirror ? px(a+rw-1) : a,b,max(1,rw),max(1,rh),color);
    };
    int headX = mirror ? px(cx+hx-1) : cx;
    uint16_t steel = MaliUI::TEXT_SECONDARY;
    if (p.head == HeadShape::Round || p.head == HeadShape::Oval) {
        tft.fillEllipse(headX+hx/2,cy,max(1,hx/2),max(1,hh/2),steel);
    } else if (p.head == HeadShape::Rectangular) {
        tft.fillRoundRect(headX,cy-hh/2,hx,hh,min(4,min(hx,hh)/2),steel);
    } else {
        int inset = min(hx/4,hh/4);
        rect(cx+inset,cy-hh/2,max(1,hx-inset*2),hh,steel);
        tft.fillTriangle(px(cx),cy,px(cx+inset),cy-hh/2,px(cx+inset),cy+hh/2,steel);
        tft.fillTriangle(px(cx+hx),cy,px(cx+hx-inset),cy-hh/2,px(cx+hx-inset),cy+hh/2,steel);
    }
    if (hx>10 && hh>10)
        tft.fillEllipse(px(cx+hx/2),cy,max(2,hx/6),max(2,hh/6),MaliUI::SURFACE);
    int body = max(1,tip-point-shoulder);
    rect(shoulder,cy-bh/2,body,bh,steel);
    tft.fillTriangle(px(tip-point),cy-bh/2,px(tip),cy,px(tip-point),cy+(bh-1)/2,steel);
    tft.drawLine(px(shoulder),cy-bh/2-2,px(shoulder),cy+(bh-1)/2+2,MaliUI::ACCENT);
    int grooveCount = cross ? 1 : p.flat.grooves;
    for (int i=0; i<grooveCount && bh>3; ++i) {
        int gy = cy-bh/2+1+(bh-2)*(i+1)/(grooveCount+1);
        tft.drawLine(px(shoulder+1),gy,px(tip-point),gy,MaliUI::SURFACE_ALT);
    }
    if (!cross && guides) {
        // Hatching identifies a serrated region. The outer edge remains straight.
        for (int a=shoulder+3; a<tip-point-2; a+=5) {
            int d = min(3,max(1,bh/3));
            if (p.flat.side != ProfileSide::Lower)
                tft.drawLine(px(a),cy-bh/2+1,px(a+2),cy-bh/2+d,MaliUI::ACCENT_DIM);
            if (p.flat.side != ProfileSide::Upper)
                tft.drawLine(px(a),cy+(bh-1)/2-1,px(a+2),cy+(bh-1)/2-d,MaliUI::ACCENT_DIM);
        }
    }
    int positions = cross ? f.positions : p.flat.positions;
    if (guides && positions) {
        int length = cross && f.length ? min(body,int(f.length*scale)) : body;
        int spacing = cross ? f.visualSpacing : 100;
        int markers = min(length,max(1,length*spacing/150));
        for (int i=0; i<positions; ++i) {
            int a=shoulder+(body-markers)/2+(positions>1 ? markers*i/(positions-1) : markers/2);
            // Position markers are outside the body and do not encode tooth shape or depths.
            tft.drawLine(px(a),cy+bh/2+4,px(a),cy+bh/2+6,MaliUI::ACCENT);
        }
    }
}
void frontView(const KeyProfile &p, int face, int x, int y, int w, int h) {
    int arms[4], widths[4];
    for (int i=0; i<4; ++i) {
        arms[i]=p.cross.faces[i].arm ? p.cross.faces[i].arm : 600;
        widths[i]=p.cross.faces[i].width ? p.cross.faces[i].width : 240;
    }
    int left=max(arms[3],max(widths[0],widths[2])/2);
    int right=max(arms[1],max(widths[0],widths[2])/2);
    int up=max(arms[0],max(widths[1],widths[3])/2);
    int down=max(arms[2],max(widths[1],widths[3])/2);
    float scale=std::min(float(w-32)/(left+right),float(h-30)/(up+down));
    int cx=x+(w-int((left+right)*scale))/2+int(left*scale);
    int cy=y+(h-int((up+down)*scale))/2+int(up*scale);
    // Paint selected arm last so the active face remains identifiable at the centre.
    for (int pass=0; pass<5; ++pass) {
        int i=pass==4 ? face : pass;
        if (pass<4 && i==face) continue;
        int a=max(2,int(arms[i]*scale)), b=max(2,int(widths[i]*scale));
        int rx=cx,ry=cy,rw=b,rh=a;
        if (i==0) {rx-=b/2;ry-=a;}
        if (i==1) {ry-=b/2;rw=a;rh=b;}
        if (i==2) rx-=b/2;
        if (i==3) {rx-=a;ry-=b/2;rw=a;rh=b;}
        tft.fillRect(rx,ry,rw,rh,i==face ? MaliUI::ACCENT : MaliUI::TEXT_SECONDARY);
    }
    text(cx-3,y+1,"A",8,face==0?MaliUI::ACCENT:MaliUI::TEXT_SECONDARY);
    text(x+w-10,cy-4,"B",8,face==1?MaliUI::ACCENT:MaliUI::TEXT_SECONDARY);
    text(cx-3,y+h-10,"C",8,face==2?MaliUI::ACCENT:MaliUI::TEXT_SECONDARY);
    text(x+2,cy-4,"D",8,face==3?MaliUI::ACCENT:MaliUI::TEXT_SECONDARY);
}
String measured(Measure value) { return value ? millimetres(value,false) : "--"; }
}
void KeyRenderer::begin(const KeyProfile &p) {
    tft.fillScreen(MaliUI::BACKGROUND);
    MaliUI::drawHeader(p.type==KeyType::Flat ? T::Flat : T::Cross);
}
void KeyRenderer::draw(const KeyProfile &p, uint8_t face, bool guides) {
    face %= 4;
    bool portrait=tftHeight>tftWidth, cross=p.type==KeyType::Cruciform;
    int x=8,y=32,w=tftWidth-16,h=tftHeight-58;
    MaliUI::drawCard(x,y,w,h,true);
    text(x+8,y+8,p.name[0]?p.name:T::Unsaved,w-16,MaliUI::TEXT_PRIMARY);
    int artY=y+24, infoY=y+h-38, infoWidth=w-16;
    if (cross) {
        bool left=(p.orientation==Orientation::Left)!=(p.cross.faces[face].orientation==Orientation::Left);
        text(x+8,artY,String(T::Face)+" "+char('A'+face)+" / "+(left?"Esquerda":"Direita"),w-16,MaliUI::ACCENT);
        artY+=14;
        int artH=infoY-artY-3;
        if (portrait) {
            sideView(p,face,x+4,artY,w-8,artH/3,guides);
            frontView(p,face,x+8,artY+artH/3,w-16,artH*2/3);
        } else {
            int frontW=min(76,w/3);
            sideView(p,face,x+4,artY,w-frontW-8,artH,guides);
            frontView(p,face,x+w-frontW-4,y+24,frontW,h-32);
            infoWidth=w-frontW-16;
        }
    } else sideView(p,face,x+4,artY,w-8,infoY-artY-4,guides);
    text(x+8,infoY,"C: "+measured(p.length)+" U: "+measured(p.useful)+" mm",infoWidth);
    text(x+8,infoY+12,"L: "+measured(p.width)+" E: "+measured(p.thickness)+" mm",infoWidth);
    text(x+8,infoY+24,T::Illustration,infoWidth,MaliUI::ACCENT);
    MaliUI::drawFooter(cross ? T::FaceNav : T::ViewNav);
}
}
