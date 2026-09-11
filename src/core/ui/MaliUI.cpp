#include "MaliUI.h"
#include "core/display.h"
#include <cmath>
#include <soc/soc_caps.h>
#if SOC_BLE_SUPPORTED
#include <NimBLEDevice.h>
#endif
namespace MaliUI {
namespace {
#ifdef HAS_SCREEN
tft_sprite strip(&tft);
bool buffered=false;
#endif
// Coordinates stay in screen space; the small backing strip clips each pass.
template<class T> struct Canvas {
    T &out; int top=0; bool clip=false;
    void line(int x,int y,int a,int b,uint16_t c){out.drawLine(x,y-top,a,b-top,c);}
    void rect(int x,int y,int w,int h,int r,uint16_t c,bool fill){
        if(fill)out.fillRoundRect(x,y-top,w,h,r,c);else out.drawRoundRect(x,y-top,w,h,r,c);
    }
    void circle(int x,int y,int r,uint16_t c,bool fill=false){if(fill)out.fillCircle(x,y-top,r,c);else out.drawCircle(x,y-top,r,c);}
    void text(const String &s,int x,int y,int size,uint16_t c,uint16_t bg=BACKGROUND){
        if(clip && (y+size*8<top || y>top+24))return;
        out.setTextDatum(0);out.setTextSize(size);out.setTextColor(c,bg);out.drawString(s,x,y-top,1);
    }
};
template<class T> void symbol(Canvas<T> &p,Icon type,int cx,int cy,int size,uint16_t c){
    int r=max(3,size/2),a=max(2,r/2);
    switch(type){
    case Icon::Mali:
        p.line(cx-r,cy+r,cx-r,cy-r,c);p.line(cx-r,cy-r,cx,cy+a,c);
        p.line(cx,cy+a,cx+r,cy-r,c);p.line(cx+r,cy-r,cx+r,cy+r,c);
        p.line(cx-r+2,cy+r,cx-r+2,cy,c);p.line(cx+r-2,cy+r,cx+r-2,cy,c);break;
    case Icon::Network:case Icon::Wifi:
        for(int k=1;k<=3;++k){int n=r*k/3;p.line(cx-n,cy-n/2,cx,cy-n,c);p.line(cx,cy-n,cx+n,cy-n/2,c);}p.circle(cx,cy+a,1,c,true);break;
    case Icon::Ble:
        p.line(cx,cy-r,cx,cy+r,c);p.line(cx,cy-r,cx+a,cy-a,c);p.line(cx+a,cy-a,cx-a,cy+a,c);
        p.line(cx-a,cy-a,cx+a,cy+a,c);p.line(cx+a,cy+a,cx,cy+r,c);break;
    case Icon::Radio:case Icon::Rf:
        p.circle(cx,cy,2,c,true);p.circle(cx,cy,r,c);p.line(cx,cy,cx,cy+r+2,c);p.line(cx-a,cy+r+2,cx+a,cy+r+2,c);break;
    case Icon::Tools:
        p.line(cx-r,cy+r,cx+r,cy-r,c);p.line(cx-r,cy+r-2,cx+r-2,cy-r,c);p.circle(cx+r-2,cy-r+2,3,c);p.circle(cx-r+2,cy+r-2,2,c);break;
    case Icon::Counter:
        for(int i=0;i<4;++i){int h=(i==2?2:i+2)*r/3;p.rect(cx-r+i*r/2,cy+r-h,max(2,r/3),h,1,c,true);}break;
    case Icon::Files:
        p.rect(cx-r,cy-a,2*r, r+a,2,c,false);p.line(cx-r,cy-a,cx-r,cy-r,c);p.line(cx-r,cy-r,cx,cy-r,c);p.line(cx,cy-r,cx+a,cy-a,c);break;
    case Icon::Nfc:
        p.rect(cx-r,cy-r,2*r,2*r,2,c,false);p.rect(cx-a,cy-a,2*a,2*a,1,c,false);break;
    case Icon::Ir:
        p.circle(cx-r,cy,2,c,true);p.line(cx-a,cy-a,cx+r,cy-r,c);p.line(cx-a,cy+a,cx+r,cy+r,c);p.line(cx-a,cy,cx+r,cy,c);break;
    default:
        p.circle(cx,cy,a,c);for(int i=0;i<8;++i){float angle=i*0.785398f;p.line(cx+int(cosf(angle)*(r-2)),cy+int(sinf(angle)*(r-2)),cx+int(cosf(angle)*(r+1)),cy+int(sinf(angle)*(r+1)),c);}break;
    }
}
struct Tile {const String *label;const char *detail;Icon icon;bool enabled;};
template<class T> void gearScene(Canvas<T> &p,const Tile *tiles,const GearMotion &motion,bool compact=false){
    const bool portrait=tftHeight>tftWidth;
    int cy=26+(tftHeight-42)/2+(portrait?15:0), spacing=portrait?108:52, gearX=portrait?tftWidth/2:39;
    int gearY=portrait?cy-61:cy, cardX=portrait?10:78, cardW=tftWidth-cardX-10;
    // Neighbours are deliberately clipped by the content viewport.
    for(int k=compact?-1:-2;k<=(compact?1:2);++k){
        float relative=k+motion.offset/1024.0f;
        float near=max(0.0f,1.0f-fabsf(relative));
        int inset=int((1-near)*9),h=portrait?int(52+near*18):int(39+near*16);
        int y=cy+int(relative*spacing)-h/2;
        bool selected=fabsf(relative)<0.5f;
        uint16_t bg=selected?SURFACE:SURFACE_ALT,color=tiles[k+2].enabled?(selected?TEXT_PRIMARY:TEXT_SECONDARY):TEXT_DISABLED;
        p.rect(cardX+inset,y,cardW-2*inset,h,MALI_RADIUS_LARGE,bg,true);
        p.rect(cardX+inset,y,cardW-2*inset,h,MALI_RADIUS_LARGE,selected?ACCENT:BORDER,false);
        int iconSize=selected?22:14,ix=cardX+inset+19;
        if(!compact||selected)symbol(p,tiles[k+2].icon,ix,y+h/2,iconSize,selected?ACCENT:TEXT_DISABLED);
        const String &label=*tiles[k+2].label;
        int font=selected&&int(label.length())*12<cardW-55?TITLE:PRIMARY;
        int tx=cardX+inset+37,ty=y+h/2-font*4-(selected?6:0);
        p.text(label,tx,ty,font,color,bg);
        if(selected)p.text(tiles[k+2].detail,tx,y+h/2+9,SECONDARY,TEXT_SECONDARY,bg);
    }
    // Small axis, not a literal large gear. Direction follows encoder steps.
    p.circle(gearX,gearY,portrait?23:27,BORDER);
    for(int i=0;i<(compact?4:12);++i){float a=(i*300+motion.phase)*0.00174532925f;int r=portrait?27:31;
        p.line(gearX+int(cosf(a)*r),gearY+int(sinf(a)*r),gearX+int(cosf(a)*(r+3)),gearY+int(sinf(a)*(r+3)),i<3?ACCENT:ACCENT_DIM);}
    symbol(p,Icon::Mali,gearX,gearY,22,ACCENT);
}
}
Icon iconFor(const String &label){
    if(label=="NETWORK"||label=="Rede"||label=="Wi-Fi")return Icon::Network;
    if(label=="RADIO"||label=="Sub-GHz"||label=="RF")return Icon::Radio;
    if(label=="TOOLS"||label=="Mali Tools"||label=="KEY GAUGE"||label=="PIXEL PAINT")return Icon::Tools;
    if(label=="COUNTER"||label=="Counter Suite")return Icon::Counter;
    if(label=="FILES"||label=="Arquivos")return Icon::Files;
    if(label.indexOf("BLE")>=0||label=="Bluetooth")return Icon::Ble;
    if(label.indexOf("NFC")>=0)return Icon::Nfc;
    if(label=="Infrared"||label=="Infravermelho")return Icon::Ir;
    return Icon::Settings;
}
const char *description(const String &label){
    if(label=="NETWORK")return "Wi-Fi / BLE / LAN";
    if(label=="RADIO")return "RF / IR / NFC";
    if(label=="TOOLS")return "Mali utilities";
    if(label=="COUNTER")return "Resilience lab";
    if(label=="FILES")return "SD / internal";
    if(label=="SYSTEM")return "Device / settings";
    if(label=="KEY GAUGE")return "Profile viewer";
    if(label=="PIXEL PAINT")return "Pixel canvas";
    if(label=="D20")return "Roll / explore";
    if(label=="Bluetooth / BLE")return "Signal / resilience";
    if(label=="Wi-Fi")return "Network diagnostics";
    if(label=="RF")return "Passive spectrum";
    if(label=="Infrared")return "Capture / timing";
    if(label=="NFC")return "Read / measure";
    return "Click to open";
}
void drawIcon(Icon icon,int x,int y,int size,uint16_t color){Canvas<tft_logger> p{tft,0};symbol(p,icon,x,y,size,color);}
void drawHeader(const String &section,bool status){
    tft.fillRect(0,0,tftWidth,25,BACKGROUND);tft.drawFastHLine(8,24,tftWidth-16,BORDER);
    tft.setTextDatum(0);tft.setTextSize(SECTION);tft.setTextColor(TEXT_PRIMARY,BACKGROUND);
    String title="MALI";if(section.length())title+=" / "+section;
    int reserve=tftWidth<240&&section.length()>10?42:76;
    tft.drawString(title.substring(0,max(4,(tftWidth-reserve)/6)),8,8,1);
    if(status){static int battery=-1;static uint32_t at=0;if(battery<0||millis()-at>10000){battery=getBattery();at=millis();}
        tft.setTextColor(TEXT_SECONDARY,BACKGROUND);tft.drawRightString(String(constrain(battery,0,100))+"%",tftWidth-8,8,1);
        if(reserve>42){
            if(wifiConnected)tft.fillCircle(tftWidth-58,12,2,ACCENT);
            if(sdcardMounted)tft.drawRoundRect(tftWidth-49,8,6,8,1,TEXT_SECONDARY);
#if SOC_BLE_SUPPORTED
            if(NimBLEDevice::isInitialized())drawIcon(Icon::Ble,tftWidth-70,12,8,ACCENT);
#endif
        }
    }
}
void drawFooter(const String &text){
    tft.fillRect(0,tftHeight-16,tftWidth,16,BACKGROUND);tft.setTextDatum(0);tft.setTextSize(FOOTER);tft.setTextColor(TEXT_SECONDARY,BACKGROUND);
    String value=text;if(tftWidth<240 && text=="Turn: select   Click: open   Hold: back")value="Turn / Click / Hold: back";
    tft.drawString(value.substring(0,(tftWidth-12)/6),6,tftHeight-12,1);
}
void drawCard(int x,int y,int w,int h,bool selected){tft.fillRoundRect(x,y,w,h,MALI_RADIUS_LARGE,SURFACE);tft.drawRoundRect(x,y,w,h,MALI_RADIUS_LARGE,selected?ACCENT:BORDER);}
void drawMenuItem(const String &label,int x,int y,int w,int h,bool selected,bool enabled){
    uint16_t bg=selected?SURFACE_ALT:BACKGROUND;
    tft.fillRoundRect(x,y,w,h,MALI_RADIUS_MEDIUM,bg);if(selected){tft.drawRoundRect(x,y,w,h,MALI_RADIUS_MEDIUM,ACCENT_DIM);tft.fillRoundRect(x+3,y+5,2,h-10,1,ACCENT);}
    tft.setTextSize(PRIMARY);tft.setTextDatum(0);tft.setTextColor(enabled?(selected?TEXT_PRIMARY:TEXT_SECONDARY):TEXT_DISABLED,bg);
    tft.drawString(label.substring(0,max(1,(w-22)/6)),x+12,y+(h-8)/2,1);
}
void drawProgress(int x,int y,int w,int value,int total,uint16_t color){
    tft.fillRoundRect(x,y,w,5,MALI_RADIUS_SMALL,SURFACE_ALT);if(total>0){int fill=int(int64_t(constrain(value,0,total))*w/total);if(fill>0)tft.fillRoundRect(x,y,fill,5,min(2,fill/2),color);}
}
void drawDialog(const String &message,uint16_t color){
    auto lines=wrapText(message,max(1,(tftWidth-40)/6));int h=min(tftHeight-40,36+int(lines.size())*12),y=(tftHeight-h)/2;
    drawCard(10,y,tftWidth-20,h);tft.fillRoundRect(16,y+12,3,h-24,1,color);tft.setTextDatum(0);tft.setTextSize(PRIMARY);tft.setTextColor(TEXT_PRIMARY,SURFACE);
    for(size_t i=0;i<lines.size()&&int(i)*12<h-24;++i)tft.drawString(lines[i],26,y+12+i*12,1);
}
void drawToast(const String &message,uint16_t color){
    if(message.length()>size_t((tftWidth-40)/6)){drawDialog(message,color);return;}
    int y=tftHeight-48;drawCard(10,y,tftWidth-20,30);tft.fillCircle(21,y+15,2,color);tft.setTextDatum(0);tft.setTextSize(PRIMARY);tft.setTextColor(TEXT_PRIMARY,SURFACE);tft.drawString(message,30,y+11,1);
}
void drawSelector(const char *const *labels,int count,int selected,int x,int y,int w){if(count<=0)return;int step=w/count;for(int i=0;i<count;++i)drawMenuItem(labels[i],x+i*step,y,step-3,25,i==selected);}
void drawTabs(const char *const *labels,int count,int selected,int y){drawSelector(labels,count,selected,8,y,tftWidth-16);}
void drawBoot(int progress,bool ready){
    if(progress==0||ready){tft.fillScreen(BACKGROUND);Canvas<tft_logger> p{tft,0};symbol(p,Icon::Mali,tftWidth/2,tftHeight/2-32,32,ACCENT);
        tft.setTextDatum(0);tft.setTextSize(TITLE);tft.setTextColor(TEXT_PRIMARY,BACKGROUND);tft.drawCentreString(ready?"MALI OS":"MALI",tftWidth/2,tftHeight/2-3,1);
        tft.setTextSize(SECONDARY);tft.setTextColor(TEXT_SECONDARY,BACKGROUND);tft.drawCentreString(ready?String("v")+MALIOS_VERSION:"Predatory Firmware",tftWidth/2,tftHeight/2+23,1);}
    drawProgress(tftWidth/4,tftHeight/2+45,tftWidth/2,progress,100);
}
bool beginGear(){
#ifdef HAS_SCREEN
    if(tft.getLogging())return false;
    if(buffered)return true;
    strip.setColorDepth(16);buffered=strip.createSprite(tftWidth,24)!=nullptr;return buffered;
#else
    return false;
#endif
}
void endGear(){
#ifdef HAS_SCREEN
    strip.deleteSprite();buffered=false;
#endif
}
void drawGearMenu(const std::vector<Option> &items,int index,const GearMotion &motion,const char *section){
    if(items.empty())return;
    Tile tiles[5];for(int k=-2;k<=2;++k){auto &item=items[wrap(index+k,items.size())];tiles[k+2]={&item.label,description(item.label),iconFor(item.label),item.enabled};}
#ifdef HAS_SCREEN
    if(buffered&&!tft.getLogging()){for(int y=26;y<tftHeight-16;y+=24){strip.fillScreen(BACKGROUND);Canvas<tft_sprite> p{strip,y,true};gearScene(p,tiles,motion);strip.pushSprite(0,y);}drawFooter();return;}
#endif
    tft.fillRect(0,26,tftWidth,tftHeight-42,BACKGROUND);Canvas<tft_logger> p{tft,0};GearMotion still;still.phase=motion.phase;gearScene(p,tiles,still,tft.getLogging());drawHeader(section);drawFooter();
}
}
