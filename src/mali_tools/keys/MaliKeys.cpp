#include "MaliKeys.h"
#include "KeyComparison.h"
#include "KeyInput.h"
#include "KeyMeasurement.h"
#include "KeyRenderer.h"
#include "KeyStorage.h"
#include "core/display.h"
#include "core/sd_functions.h"
#include "core/ui/KeysPtBr.h"
#include "modules/mali/MaliWiki.h"
#include <cstring>
#include <time.h>

namespace MaliKeys {
namespace T = MaliText::Keys;
namespace {
const char *typeName(KeyType type) { return type == KeyType::Flat ? T::Flat : T::Cross; }
String pair(const char *label, const String &value) { return String(label)+": "+value; }
String optional(const char *text) { return text[0] ? String(text) : String(T::Unknown); }
String measure(Measure value) { return value ? millimetres(value) : String(T::Unknown); }
bool confirm(const char *question, const String &name = "") {
    bool yes = false;
    std::vector<Option> options = {{T::Cancel, [](){}}, {T::Confirm, [&](){yes=true;}}};
    String title=String(question)+(name.length() ? " "+name : "");
    loopOptions(options, MENU_TYPE_GEAR, title.c_str());
    return yes && !returnToMenu;
}
int choose(const char *title, const std::vector<String> &labels, int current=0) {
    int chosen=-1;
    std::vector<Option> options; options.reserve(labels.size()+1);
    for (size_t i=0;i<labels.size();++i) options.push_back({labels[i],[&,i](){chosen=int(i);}});
    options.push_back({T::Back, [](){}});
    loopOptions(options, MENU_TYPE_GEAR, title, current);
    return chosen;
}
void report(StoreResult result) {
    if (result != StoreResult::Ok) showText(T::Title, T::Errors[int(result)]);
}
class Session {
    KeyStorage storage;
    KeyProfile profile;
    bool hasProfile=false, dirty=false, saved=false, guides=true;
    uint8_t face=0;
    int step=10; // 0.10 mm; session setting, always displayed as such.
    const bool sd;
    void editMeasure(const char *title, Measure &field, int hi) {
        int n=field;
        if (editNumber(title,n,0,hi,step,true) && n!=field) {field=Measure(n);dirty=true;}
    }
    void editCount(const char *title, uint8_t &field, int lo, int hi) {
        int n=field;
        if (editNumber(title,n,lo,hi) && n!=field) {field=uint8_t(n);dirty=true;}
    }
    template<size_t N> void editString(const char *title, char (&field)[N]) {
        char candidate[N]; memcpy(candidate,field,N);
        if (editText(title,candidate,N) && strcmp(candidate,field)) {
            memcpy(field,candidate,N); dirty=true;
        }
    }
    void editDate() {
        char value[11]; memcpy(value,profile.created,sizeof(value));
        if (!editText(T::Date,value,sizeof(value))) return;
        if (!validDate(value)) {showText(T::Date,T::InvalidDate);return;}
        if (strcmp(value,profile.created)) {memcpy(profile.created,value,sizeof(value));dirty=true;}
    }
    void editOrientation(Orientation &orientation) {
        int choice=choose(T::Direction,{T::Directions[0],T::Directions[1]},int(orientation));
        if (choice>=0 && choice!=int(orientation)) {orientation=Orientation(choice);dirty=true;}
    }
    bool abandon() { return !dirty || confirm(T::Discard); }
    void create(KeyType type) {
        if (!abandon()) return;
        profile=KeyProfile{};profile.type=type;hasProfile=true;dirty=false;saved=false;face=0;
        time_t now=time(nullptr); struct tm date{};
        if (now>1577836800 && localtime_r(&now,&date))
            strftime(profile.created,sizeof(profile.created),"%Y-%m-%d",&date);
        actions();
    }
    void save(bool asCopy) {
        KeyProfile candidate=profile;
        bool replace=saved && !asCopy;
        if (!replace) {
            if (!editText(T::Name,candidate.name,sizeof(candidate.name))) return;
            if (!validName(candidate.name)) {showText(T::Name,T::InvalidName);return;}
        }
        if (!validProfile(candidate)) {showText(T::Measure,T::InvalidMeasures);return;}
        if (replace && !confirm(T::Replace,candidate.name)) return;
        StoreResult result=storage.save(candidate,replace);
        if (result!=StoreResult::Ok) {report(result);return;}
        profile=candidate;saved=true;dirty=false;
        showText(T::Save,T::Saved);
    }
    void measurements() {
        while (!returnToMenu) {
            bool back=false;
            std::vector<Option> options={
                {pair(T::Length,measure(profile.length)),[&](){
                    // Bounds on edit prevent invalidating previously measured dependent dimensions.
                    int n=profile.length;
                    if (editNumber(T::Length,n,0,30000,step,true)) {
                        if (n && profile.useful>n) showText(T::Measure,T::InvalidMeasures);
                        else if(n!=profile.length){profile.length=n;dirty=true;}
                    }
                }},
                {pair(T::Useful,measure(profile.useful)),[&](){
                    int n=profile.useful;
                    if (editNumber(T::Useful,n,0,profile.length?profile.length:30000,step,true)) {
                        bool valid=true;
                        for(const auto &f:profile.cross.faces) if(n && f.length>n) valid=false;
                        if(!valid) showText(T::Measure,T::InvalidMeasures);
                        else if(n!=profile.useful){profile.useful=n;dirty=true;}
                    }
                }},
                {pair(T::Width,measure(profile.width)),[&](){editMeasure(T::Width,profile.width,10000);}},
                {pair(T::Thickness,measure(profile.thickness)),[&](){editMeasure(T::Thickness,profile.thickness,5000);}},
            };
            if(profile.type==KeyType::Cruciform)
                options.push_back({T::FaceSize,[&](){faces(true);}});
            options.push_back({T::Help,[](){showText(T::Help,T::MeasureHelp);}});
            options.push_back({T::Back,[&](){back=true;}});
            if(loopOptions(options,MENU_TYPE_GEAR,T::Measure)<0 || back) return;
        }
    }
    void faceEditor(bool measuresOnly) {
        while(!returnToMenu) {
            auto &f=profile.cross.faces[face];bool back=false;
            std::vector<Option> options={
                {pair(T::FaceLength,measure(f.length)),[&](){editMeasure(T::FaceLength,f.length,profile.useful?profile.useful:30000);}},
                {pair(T::Arm,measure(f.arm)),[&](){editMeasure(T::Arm,f.arm,5000);}},
                {pair(T::ArmWidth,measure(f.width)),[&](){editMeasure(T::ArmWidth,f.width,5000);}},
            };
            if(!measuresOnly) {
                options.push_back({pair(T::Positions,String(f.positions)),[&](){editCount(T::Positions,f.positions,0,20);}});
                options.push_back({pair(T::Spacing,String(f.visualSpacing)),[&](){editCount(T::Spacing,f.visualSpacing,50,150);}});
                options.push_back({T::Direction,[&](){editOrientation(f.orientation);}});
                options.push_back({T::Notes,[&](){editString(T::Notes,f.notes);}});
                options.push_back({T::View,[&](){view();}});
            }
            options.push_back({T::Back,[&](){back=true;}});
            String title=String(T::Face)+" "+char('A'+face);
            if(loopOptions(options,MENU_TYPE_GEAR,title.c_str())<0 || back) return;
        }
    }
    void faces(bool measuresOnly=false) {
        while(!returnToMenu) {
            int selected=choose(T::Face,{"Face A","Face B","Face C","Face D"},face);
            if(selected<0) return;
            face=selected;faceEditor(measuresOnly);
        }
    }
    void edit() {
        while(!returnToMenu) {
            bool back=false;
            std::vector<Option> options={
                {T::Measure,[&](){measurements();}},
                {T::Maker,[&](){editString(T::Maker,profile.manufacturer);}},
                {T::Profile,[&](){editString(T::Profile,profile.profileType);}},
                {T::Head,[&](){
                    int n=choose(T::Head,{T::Heads[0],T::Heads[1],T::Heads[2],T::Heads[3]},int(profile.head));
                    if(n>=0 && n!=int(profile.head)){profile.head=HeadShape(n);dirty=true;}
                }},
                {T::Direction,[&](){editOrientation(profile.orientation);}},
                {T::Notes,[&](){editString(T::Notes,profile.notes);}},
                {T::Date,[&](){editDate();}},
            };
            if(profile.type==KeyType::Flat) {
                options.push_back({pair(T::Positions,String(profile.flat.positions)),[&](){editCount(T::Positions,profile.flat.positions,0,20);}});
                options.push_back({T::Side,[&](){
                    int n=choose(T::Side,{T::Sides[0],T::Sides[1],T::Sides[2]},int(profile.flat.side));
                    if(n>=0 && n!=int(profile.flat.side)){profile.flat.side=ProfileSide(n);dirty=true;}
                }});
                options.push_back({pair(T::Grooves,String(profile.flat.grooves)),[&](){editCount(T::Grooves,profile.flat.grooves,0,8);}});
            } else options.push_back({T::Face,[&](){faces();}});
            options.push_back({T::Back,[&](){back=true;}});
            if(loopOptions(options,MENU_TYPE_GEAR,T::Edit)<0 || back) return;
        }
    }
    void details() {
        String text=pair(T::Name,optional(profile.name))+"\n"+pair(T::Type,typeName(profile.type))+"\n"+
            pair(T::Maker,optional(profile.manufacturer))+"\n"+pair(T::Profile,optional(profile.profileType))+"\n"+
            pair(T::Date,optional(profile.created))+"\n"+pair(T::Length,measure(profile.length))+"\n"+
            pair(T::Useful,measure(profile.useful))+"\n"+pair(T::Width,measure(profile.width))+"\n"+
            pair(T::Thickness,measure(profile.thickness))+"\n"+pair(T::Head,T::Heads[int(profile.head)])+"\n"+
            pair(T::Direction,T::Directions[int(profile.orientation)])+"\n"+pair(T::Notes,optional(profile.notes));
        if(profile.type==KeyType::Flat) {
            text+="\n"+pair(T::Positions,profile.flat.positions?String(profile.flat.positions):String(T::Unknown));
            text+="\n"+pair(T::Side,T::Sides[int(profile.flat.side)])+"\n"+pair(T::Grooves,String(profile.flat.grooves));
        } else for(int i=0;i<4;++i) {
            const auto &f=profile.cross.faces[i];
            text+="\n\n"+String(T::Face)+" "+char('A'+i)+"\n"+pair(T::Positions,f.positions?String(f.positions):String(T::Unknown));
            text+="\n"+pair(T::Spacing,String(f.visualSpacing))+"\n"+pair(T::FaceLength,measure(f.length));
            text+="\n"+pair(T::Arm,measure(f.arm))+"\n"+pair(T::ArmWidth,measure(f.width));
            text+="\n"+pair(T::Direction,T::Directions[int(f.orientation)])+"\n"+pair(T::Notes,optional(f.notes));
        }
        showText(T::Details,text);
    }
    void view() {
        KeyRenderer::begin(profile);KeyRenderer::draw(profile,face,guides);
        KeyInput input;
        while(!returnToMenu) {
            auto e=input.read();if(e.back || e.select) return;
            if(e.steps && profile.type==KeyType::Cruciform) {
                face=MaliUI::wrap(int64_t(face)+e.steps,4);
                KeyRenderer::draw(profile,face,guides);
            }
            delay(5);
        }
    }
    void actions() {
        while(!returnToMenu) {
            bool back=false;
            std::vector<Option> options={
                {T::View,[&](){view();}}, {T::Measure,[&](){measurements();}},
                {T::Edit,[&](){edit();}}, {T::Details,[&](){details();}},
                {T::Save,[&](){save(false);}}, {T::SaveAs,[&](){save(true);}},
                {T::Back,[&](){back=true;}}
            };
            String title=String(dirty?"* ":"")+typeName(profile.type);
            if(loopOptions(options,MENU_TYPE_GEAR,title.c_str())<0 || back) return;
        }
    }
    bool selectRecord(const char *title, KeyProfile &out, const String &exclude="") {
        std::vector<String> names;
        StoreResult result=storage.list(names);
        if(result!=StoreResult::Ok){report(result);return false;}
        std::vector<Option> options;bool loaded=false;
        for(const auto &name:names) if(name!=exclude) options.push_back({name,[&,name](){
            StoreResult r=storage.load(name,out);report(r);loaded=r==StoreResult::Ok;
        }});
        if(options.empty()){showText(T::Catalog,T::Empty);return false;}
        options.push_back({T::Back,[](){}});
        loopOptions(options,MENU_TYPE_REGULAR,title);
        return loaded;
    }
    void compare() {
        std::vector<String> names;StoreResult result=storage.list(names);
        if(result!=StoreResult::Ok){report(result);return;}
        if(names.size()<2){showText(T::Compare,T::NeedTwo);return;}
        KeyProfile a,b;
        if(!selectRecord("Perfil A",a) || !selectRecord("Perfil B",b,a.name)) return;
        String text=String(a.name)+"\nvs\n"+b.name+"\n\n"+T::Delta+"\n";
        text+=pair(T::Length,difference(a.length,b.length))+"\n"+pair(T::Useful,difference(a.useful,b.useful));
        text+="\n"+pair(T::Width,difference(a.width,b.width))+"\n"+pair(T::Thickness,difference(a.thickness,b.thickness));
        int pa=positionCount(a),pb=positionCount(b);
        text+="\n"+pair(T::Positions,pa && pb ? String(pa-pb) : String(T::Unknown));
        text+="\n"+pair(T::Type,a.type==b.type?T::Similar:T::Different);
        text+="\nA: "+String(typeName(a.type))+" / B: "+typeName(b.type);
        text+="\n"+pair(T::Shape,a.head==b.head?T::Similar:T::Different);
        text+="\nA: "+String(T::Heads[int(a.head)])+" / B: "+T::Heads[int(b.head)];
        text+="\n"+pair(T::Profile,optional(a.profileType)+" / "+optional(b.profileType));
        if(a.type==KeyType::Cruciform && b.type==KeyType::Cruciform) for(int i=0;i<4;++i) {
            const auto &fa=a.cross.faces[i];const auto &fb=b.cross.faces[i];
            text+="\n\n"+String(T::Face)+" "+char('A'+i);
            text+="\n"+pair(T::FaceLength,difference(fa.length,fb.length));
            text+="\n"+pair(T::Arm,difference(fa.arm,fb.arm))+"\n"+pair(T::ArmWidth,difference(fa.width,fb.width));
            text+="\n"+pair(T::Positions,fa.positions && fb.positions ? String(int(fa.positions)-fb.positions) : String(T::Unknown));
        }
        text+="\n\n"+String(T::CompareInfo);showText(T::Compare,text);
    }
    void recordMenu(const String &name) {
        bool back=false;
        while(!back && !returnToMenu) {
            std::vector<Option> options={
                {T::View,[&](){
                    if(!abandon())return;
                    KeyProfile candidate;StoreResult r=storage.load(name,candidate);report(r);
                    if(r==StoreResult::Ok){profile=candidate;hasProfile=true;saved=true;dirty=false;face=0;view();actions();}
                }},
                {T::Rename,[&](){
                    char newName[32];name.toCharArray(newName,sizeof(newName));
                    if(!editText(T::Rename,newName,sizeof(newName)))return;
                    if(!validName(newName)){showText(T::Name,T::InvalidName);return;}
                    StoreResult r=storage.rename(name,newName);report(r);
                    if(r==StoreResult::Ok){
                        if(saved && name==profile.name)strlcpy(profile.name,newName,sizeof(profile.name));
                        showText(T::Rename,T::Renamed);back=true;
                    }
                }},
                {T::Delete,[&](){
                    if(!confirm(T::DeleteQuestion,name))return;
                    StoreResult r=storage.remove(name);report(r);
                    if(r==StoreResult::Ok){
                        if(saved && name==profile.name){saved=false;dirty=true;}
                        showText(T::Delete,T::Deleted);back=true;
                    }
                }},
                {T::Back,[&](){back=true;}}
            };
            if(loopOptions(options,MENU_TYPE_GEAR,name.c_str())<0)return;
        }
    }
    void catalog() {
        while(!returnToMenu) {
            std::vector<String> names;StoreResult result=storage.list(names);
            if(result!=StoreResult::Ok){report(result);return;}
            bool back=false;std::vector<Option> options;
            options.push_back({T::Compare,[&](){compare();}});
            for(const auto &name:names)options.push_back({name,[&,name](){recordMenu(name);}});
            options.push_back({T::Back,[&](){back=true;}});
            if(loopOptions(options,MENU_TYPE_REGULAR,T::Catalog)<0 || back)return;
        }
    }
    void settings() {
        while(!returnToMenu) {
            bool back=false;
            std::vector<Option> options={
                {pair(T::Step,millimetres(step)),[&](){
                    int n=choose(T::Session,{"0,01 mm","0,10 mm","1,00 mm"},step==1?0:step==10?1:2);
                    if(n>=0)step=n==0?1:n==1?10:100;
                }},
                {pair(T::Guides,guides?T::On:T::Off),[&](){guides=!guides;}},
                {T::Storage,[&](){showText(T::Storage,String(sd?T::Sd:T::Internal)+"\n/MaliKeys/\nArquivos .mkey\n"+T::Session);}},
                {T::Help,[](){showText(T::Help,T::MeasureHelp);}},
                {T::Back,[&](){back=true;}}
            };
            if(loopOptions(options,MENU_TYPE_GEAR,T::Settings)<0 || back)return;
        }
    }
public:
    Session(FS &fs,bool onSd):storage(fs),sd(onSd){}
    void run() {
        while(!returnToMenu) {
            bool back=false;
            std::vector<Option> options={
                {T::Flat,[&](){create(KeyType::Flat);}},
                {T::Cross,[&](){create(KeyType::Cruciform);}},
                {T::Measure,[&](){
                    if(!hasProfile) {
                        int type=choose(T::Type,{T::Flat,T::Cross});if(type<0)return;
                        profile=KeyProfile{};profile.type=KeyType(type);hasProfile=true;saved=false;dirty=false;face=0;
                    }
                    measurements();
                }},
                {T::Catalog,[&](){catalog();}},
                {T::Settings,[&](){settings();}},
                {"? Ajuda",[](){MaliWiki::open(MaliWiki::Category::MALI_KEYS);}}
            };
            if(hasProfile)options.push_back({T::Resume,[&](){actions();}});
            options.push_back({T::Back,[&](){back=true;}});
            int result=loopOptions(options,MENU_TYPE_GEAR,T::Title);
            if((result<0 || back) && abandon())return;
        }
    }
};
}
void open() {
    // Pin this session to one filesystem: mounting changes never redirect pending writes.
    if(!sdcardMounted)setupSdCard();
    bool onSd=sdcardMounted;
    FS &fs=onSd?static_cast<FS &>(SD):static_cast<FS &>(LittleFS);
    Session session(fs,onSd);session.run();
}
}
