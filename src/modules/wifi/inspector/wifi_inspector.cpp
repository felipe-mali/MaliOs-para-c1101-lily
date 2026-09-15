#include "wifi_inspector.h"
#include "device_scanner.h"
#include "device_database.h"
#include "device_classifier.h"
#include "oui_database.h"
#include "router_manager.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/scrollableTextArea.h"
#include "core/ui/MaliInput.h"
#include "core/ui/InspectorPtBr.h"
#include "core/wifi/wifi_common.h"
#include "modules/mali/MaliWiki.h"
#include <algorithm>
#include <memory>
#include <ctime>

namespace WifiInspector {
namespace {
namespace T=MaliText::Inspector;
String safe(const char *s){char text[97];cleanText(text,sizeof(text),s);return text;}
String stamp(uint32_t epoch){
    if(!epoch)return "Sem hora sincronizada";
    time_t value=epoch;tm local{};localtime_r(&value,&local);char s[25];strftime(s,sizeof(s),"%d/%m/%Y %H:%M",&local);return s;
}
bool show(const char *title,const String &body){
    tft.fillScreen(MaliUI::BACKGROUND);MaliUI::drawHeader(title);MaliUI::drawFooter("Girar:ler Clique:voltar");
    ScrollableTextArea area(1,8,34,tftWidth-16,tftHeight-60,false);area.fromString(body);area.draw(true);MaliUI::Input input;
    while(!returnToMenu){auto e=input.read();if(e.back)return false;if(e.select)return true;if(e.steps){int64_t line=int64_t(area.firstVisibleLine)+e.steps;int last=std::max(0,int(area.getMaxLines())-(tftHeight-60)/10);area.scrollToLine(size_t(std::max(int64_t(0),std::min(line,int64_t(last)))));area.draw(true);}delay(5);}
    return false;
}
int choose(const char *title,const std::vector<String> &labels){
    int choice=-1;std::vector<Option> menu;menu.reserve(labels.size()+1);
    for(size_t i=0;i<labels.size();++i)menu.push_back({labels[i],[&,i](){choice=int(i);}});
    menu.push_back({T::Back,[](){}});loopOptions(menu,MENU_TYPE_SUBMENU,title);return choice;
}
bool confirm(const String &message){
    if(!show("Confira antes de confirmar",message)||returnToMenu)return false;
    bool yes=false;std::vector<Option> menu={{T::Cancel,[](){}},{T::Confirm,[&](){yes=true;}}};
    loopOptions(menu,MENU_TYPE_SUBMENU,"Confirmar acao");return yes;
}
String identity(const Device &d){return String(d.name[0]?d.name:manufacturer(d.mac))+"\nIP: "+ipText(d.ip)+"\nMAC: "+(validMac(d.mac)?macText(d.mac):String(T::Missing));}
String status(const Device &d){return d.known?"CONHECIDO":d.isNew?"[!] NOVO DISPOSITIVO":"NAO IDENTIFICADO";}
int pickDevice(const char *title,const std::vector<Device *> &items,bool baseline=false){
    int count=int(items.size())+(baseline?1:0);
    if(!count){show(title,"Nenhum dispositivo nesta lista. Execute Escanear Rede; hosts isolados ou sem resposta podem nao aparecer.");return -1;}
    int selected=0;bool dirty=true;MaliUI::Input input;
    while(!returnToMenu){
        if(dirty){
            tft.fillScreen(MaliUI::BACKGROUND);MaliUI::drawHeader(String(title)+" ("+String(items.size())+")");
            int rows=std::max(1,(tftHeight-62)/34),first=selected/rows*rows;
            for(int i=first;i<count&&i<first+rows;++i){
                int y=34+(i-first)*34;MaliUI::drawCard(6,y,tftWidth-12,31,i==selected);
                tft.setTextSize(1);tft.setTextDatum(0);tft.setTextColor(i==selected?MaliUI::TEXT_PRIMARY:MaliUI::TEXT_SECONDARY,MaliUI::SURFACE);
                String top,bottom;
                if(baseline&&i==0){top="Criar Baseline Atual";bottom="Marcar encontrados como conhecidos";}
                else{const Device &d=*items[i-(baseline?1:0)];top=String(d.known?"[V] ":d.isNew?"[!] ":"[?] ")+ipText(d.ip);bottom=d.name[0]?d.name:manufacturer(d.mac);if(d.ip==0)top="IP nao informado";}
                tft.drawString(MaliUI::fitText(top,tftWidth-28),14,y+4,1);tft.drawString(MaliUI::fitText(bottom,tftWidth-28),14,y+17,1);
            }
            MaliUI::drawFooter("Girar:escolher Seg:voltar");dirty=false;
        }
        auto e=input.read();if(e.back)return -1;if(e.select)return baseline?(selected==0?-2:selected-1):selected;
        if(e.steps){selected=MaliUI::wrap(int64_t(selected)+e.steps,count);dirty=true;}delay(5);
    }
    return -1;
}
class Session {
    std::unique_ptr<Database> db;
    std::unique_ptr<ScanResult> scan;
    std::unique_ptr<DeviceStore> store;
    ScanSettings settings;
    GenericRouter router;
    uint32_t routerIp=0;
    size_t previousCount=0;
    bool hasScan=false,storageValid=true;
    bool connected(){Network n;return currentNetwork(n)&&db->network.same(n)&&n.local==db->network.local&&n.gateway==db->network.gateway;}
    void save(){if(!storageValid){show("Historico protegido","Arquivo invalido ou armazenamento indisponivel. Preserve uma copia e use Limpar historico desta rede para liberar uma base nova.");return;}if(!store->save(*db))show("Armazenamento",T::StorageError);}
    void networkInfo(){
        const auto &n=db->network;
        show("Rede conectada","T-Embed: "+ipText(n.local)+"\nMascara: "+ipText(n.mask)+"\nGateway: "+ipText(n.gateway)+"\nRede: "+ipText(n.address())+"/"+String(prefixLength(n.mask))+"\nSSID: "+safe(n.ssid)+"\nBSSID: "+macText(n.bssid)+"\nDNS: "+ipText(n.dns)+"\n\nScan: ate 254 IPs por faixa; ate 64 dispositivos. Historico: 96 MACs por rede/AP. MAC privado nao identifica permanentemente uma pessoa ou aparelho.");
    }
    void runScan(){
        if(!connected()){show(T::Title,T::NetworkChanged);return;}
        if(!scan)scan.reset(new(std::nothrow) ScanResult);
        if(!scan){show(T::Title,"Memoria insuficiente para iniciar o scan.");return;}
        previousCount=hasScan?scan->count:0;hasScan=true;
        tft.fillScreen(MaliUI::BACKGROUND);MaliUI::drawHeader(T::Title);MaliUI::drawFooter("Clique/segurar:cancelar");
        MaliUI::Input input;uint32_t drawn=millis()-150;
        DeviceScanner scanner;
        scanner.run(db->network,settings,*scan,[&](const Progress &p){
            auto e=input.read();if(e.back||e.select)return false;
            if(uint32_t(millis()-drawn)>=150||p.percent==100){
                drawn=millis();MaliUI::drawCard(8,34,tftWidth-16,tftHeight-61,true);tft.setTextSize(1);tft.setTextDatum(0);tft.setTextColor(MaliUI::TEXT_PRIMARY,MaliUI::SURFACE);
                tft.drawString(MaliUI::fitText(p.phase,tftWidth-32),16,44,1);
                tft.drawString("Dispositivos: "+String(p.found),16,61,1);tft.drawString("IP: "+(p.ip?ipText(p.ip):String("--")),16,78,1);
                tft.drawString(String(p.percent)+"%",16,95,1);MaliUI::drawProgress(16,tftHeight-43,tftWidth-32,p.percent,100);
            }
            return true;
        });
        // A roaming or DHCP change aborts the scan; never merge observations into another network.
        if(!connected()){show(T::Title,T::NetworkChanged);scan->count=0;hasScan=false;return;}
        time_t now=time(nullptr);uint32_t epoch=now>=1704067200&&uint64_t(now)<=UINT32_MAX?uint32_t(now):0;
        db->merge(*scan,epoch);if(storageValid)save();summary();
    }
    void summary(){
        if(!hasScan){networkInfo();return;}
        size_t fresh=0,entered=0,missing=0;
        for(size_t i=0;i<scan->count;++i){auto &d=scan->devices[i];if(d.isNew)++fresh;if(!d.previous)++entered;}
        for(size_t i=0;i<db->count;++i)if(db->devices[i].previous&&!db->devices[i].present)++missing;
        String text="Anterior: "+String(previousCount)+"\nAtual: "+String(scan->count)+"\nNovos MACs: "+String(fresh)+"\nEncontrados sem presenca anterior: "+String(entered)+"\nNao encontrados nesta varredura: "+String(missing)+"\nTempo: "+String(scan->elapsed/1000)+" s\nFaixa: "+ipText(scan->first)+" - "+ipText(scan->last);
        text+=scan->complete?"\nScan concluido na faixa indicada.":"\nScan parcial/cancelado: nao conclui ausencias.";
        if(scan->limited)text+="\nFaixa ou quantidade limitada. Nao cobre toda a rede.";
        if(db->full)text+="\nHistorico cheio: novos MACs excedentes nao foram gravados.";
        size_t listed=0;
        text+="\n\n+ Encontrados sem presenca anterior:";
        for(size_t i=0;i<scan->count&&listed<12;++i){const auto &d=scan->devices[i];if(!d.previous){text+="\n+ "+ipText(d.ip)+" "+String(d.name[0]?d.name:manufacturer(d.mac));++listed;}}
        if(entered>listed)text+="\nVeja os demais em Dispositivos Encontrados.";
        listed=0;text+="\n\n- Nao encontrados nesta faixa:";
        for(size_t i=0;i<db->count&&listed<12;++i){const auto &d=db->devices[i];if(d.previous&&!d.present){text+="\n- "+ipText(d.ip)+" "+String(d.name[0]?d.name:manufacturer(d.mac));++listed;}}
        if(missing>listed)text+="\nVeja os demais no Historico.";
        text+="\n\nARP pode incluir cache recente. Sem resposta nao significa saida definitiva. Redes com isolamento de clientes limitam a descoberta.";show("Resultado do scan",text);
    }
    void detail(Device &d){
        while(!returnToMenu){
            String text=identity(d)+"\nFabricante: "+manufacturer(d.mac)+"\nTipo provavel: "+classify(d,db->network.gateway)+"\nUltimo hostname: "+(d.hostname[0]?d.hostname:T::Missing)+"\nUltimo mDNS: "+(d.mdns[0]?d.mdns:T::Missing)+"\nUltimo anuncio: "+(d.advertised[0]?d.advertised:T::Missing)+"\nStatus: "+status(d);
            text+=d.present?"\nEncontrado nesta sessao (pode incluir cache).":"\nNao observado atualmente nesta sessao.";
            if(privateMac(d.mac))text+="\nMAC privado/aleatorio (localmente administrado). Pode mudar e nao identifica uma pessoa.";
            text+="\n\nPortas TCP que responderam:";bool any=false;
            for(size_t i=0;i<PORT_COUNT;++i)if(d.ports&(1u<<i)){text+="\n"+String(PORTS[i])+" "+SERVICES[i];any=true;}
            if(!any)text+="\nNenhuma observada; portas filtradas podem nao responder.";
            text+="\n\nEvidencias:";
            if(d.evidence&Arp)text+=" ARP/cache";
            if(d.evidence&Tcp)text+=" TCP";
            if(d.evidence&Dns)text+=" DNS";
            if(d.evidence&Mdns)text+=" mDNS";
            if(d.evidence&Ssdp)text+=" SSDP";
            text+="\nPrimeiro visto: "+stamp(d.firstSeen)+"\nUltimo visto: "+stamp(d.lastSeen)+"\nScans em que foi visto: "+String(d.sightings)+"\nObservacao: "+(d.note[0]?d.note:T::Missing)+"\n\nPorta sugere servico; nao confirma software. Nome anunciado e uma declaracao do equipamento.";
            if(!show(T::Details,text)||returnToMenu)return;
            int action=choose("Dispositivo",{T::Manage,"Ler detalhes novamente"});if(action==0)manage(d);else if(action!=1)return;
        }
    }
    void sync(const Device &d){if(scan)for(size_t i=0;i<scan->count;++i)if(scan->devices[i].mac==d.mac)scan->devices[i]=d;}
    void manage(Device &displayed){
        Device *d=db->find(displayed.mac);
        if(!d){show(T::Manage,"Sem MAC persistente nesta observacao, ou historico cheio. Nao e seguro vincular identidade apenas ao IP. Atualize o scan para tentar obter o MAC.");return;}
        while(!returnToMenu){
            int choice=choose(T::Manage,{d->known?"Remover de conhecidos":"Marcar como conhecido","Renomear","Adicionar observacao","Desconectar da rede"});
            if(choice<0)return;
            if(choice==0){d->known=!d->known;db->dirty=true;}
            if(choice==1||choice==2){
                char *dest=choice==1?d->name:d->note;size_t size=choice==1?sizeof(d->name):sizeof(d->note);
                String value=keyboard(String(dest),size-1,choice==1?"Nome do dispositivo":"Observacao");
                if(returnToMenu)return;
                char clean[96];cleanText(clean,size,value.c_str());if(strcmp(dest,clean)){memcpy(dest,clean,strlen(clean)+1);db->dirty=true;}
            }
            if(choice==3){
                if(!connected()){show(T::Manage,T::NetworkChanged);continue;}
                if(!hasScan||!scan->find(d->ip)){show(T::Manage,"Atualize o scan e selecione uma observacao atual deste dispositivo.");continue;}
                if(confirm("Desconectar este dispositivo?\n"+identity(*d)+"\n\nApenas encerrar a associacao atual pelo roteador. O cliente podera reconectar normalmente.")){
                    if(!router.isManagementSupported())show(T::Manage,T::Unsupported);
                    else {
                        auto result=router.authenticate();
                        if(result==ManagementResult::Authenticated)result=router.disconnectClient(d->mac);
                        router.logout();
                        show(T::Manage,result==ManagementResult::Disconnected?"Dispositivo desconectado pelo roteador.":result==ManagementResult::Requested?"Solicitacao aceita. Desconexao ainda nao verificada.":result==ManagementResult::Unsupported?T::Unsupported:"Falha administrativa. Nenhuma alternativa por radio foi executada.");
                    }
                }
            }
            sync(*d);displayed=*d;save();
        }
    }
    void baseline(){
        if(!hasScan||!connected()){show(T::Known,"Execute um scan nesta rede antes de criar a referencia.");return;}
        size_t count=0;for(size_t i=0;i<scan->count;++i)if(db->find(scan->devices[i].mac))++count;
        if(!count){show(T::Known,"Nenhum MAC disponivel para marcar.");return;}
        if(!confirm("Criar baseline desta rede?\n"+String(count)+" dispositivos encontrados serao marcados como conhecidos.\n\nConfira os aparelhos antes de confiar na lista."))return;
        for(size_t i=0;i<scan->count;++i){Device *d=db->find(scan->devices[i].mac);if(d){d->known=true;scan->devices[i].known=true;}}
        db->dirty=true;save();
    }
    void list(int mode,bool directManage=false){
        while(!returnToMenu){
            std::vector<Device *> items;items.reserve(MAX_HISTORY);
            if(mode==0||mode==2){if(scan)for(size_t i=0;i<scan->count;++i)if(mode==0||scan->devices[i].isNew)items.push_back(&scan->devices[i]);}
            else for(size_t i=0;i<db->count;++i)if(mode==3||db->devices[i].known)items.push_back(&db->devices[i]);
            const char *title=mode==0?T::Found:mode==1?T::Known:mode==2?T::New:T::History;
            int chosen=pickDevice(title,items,mode==1);
            if(chosen==-2){baseline();continue;}if(chosen<0)return;
            if(directManage)manage(*items[chosen]);else detail(*items[chosen]);
        }
    }
    void routerConfig(){
        while(!returnToMenu){
            int choice=choose("Gerenciamento do Roteador",{"IP: "+ipText(routerIp),"Tipo: Generico","Testar suporte administrativo","Credenciais","Apagar credenciais"});
            if(choice<0)return;
            if(choice==0){String text=num_keyboard(ipText(routerIp),15,"IPv4 do roteador");IPAddress ip;if(ip.fromString(text)){uint32_t value=uint32_t(ip[0])<<24|uint32_t(ip[1])<<16|uint32_t(ip[2])<<8|ip[3];if(db->network.contains(value))routerIp=value;else show("IP invalido","Use IPv4 da sub-rede atual.");}else show("IP invalido","Exemplo: 192.168.15.1. Use o endereco real do seu roteador.");}
            else if(choice==4){router.logout();show("Credenciais","Nenhuma credencial armazenada por esta versao. Sessao administrativa encerrada.");}
            else if(choice==3)show("Credenciais","O adaptador generico nao autentica. Nenhum usuario ou senha e solicitado, gravado ou enviado. Um adaptador futuro precisa validar HTTPS e guardar segredos fora dos arquivos exportaveis.");
            else show("Suporte administrativo",String(T::Unsupported)+"\n\nEsta versao oferece apenas o adaptador Generico. IP configurado vale nesta sessao. Nao ha API universal de desconexao; nenhum comando foi enviado.");
        }
    }
    void configure(){
        while(!returnToMenu){
            int choice=choose(T::Settings,{"Dados da rede","Portas TCP: "+String(settings.tcp?"LIG":"DESL"),"Nomes DNS/mDNS: "+String(settings.names?"LIG":"DESL"),"Descoberta SSDP: "+String(settings.ssdp?"LIG":"DESL"),"Timeout TCP: "+String(settings.timeoutMs)+" ms","Inicio da faixa","Gerenciamento do Roteador","Salvar historico","Limpar historico desta rede"});
            if(choice<0)return;
            switch(choice){
                case 0:networkInfo();break;
                case 1:settings.tcp=!settings.tcp;break;
                case 2:settings.names=!settings.names;break;
                case 3:settings.ssdp=!settings.ssdp;break;
                case 4:{int index=choose("Timeout TCP",{"80 ms","120 ms","200 ms"});if(index>=0)settings.timeoutMs=index==0?80:index==1?120:200;break;}
                case 5:{String value=num_keyboard(settings.start?ipText(settings.start):ipText((prefixLength(db->network.mask)<24?(db->network.local&0xffffff00u):db->network.address())+1),15,"Inicio (ate 254 IPs)");IPAddress ip;if(ip.fromString(value)){uint32_t n=uint32_t(ip[0])<<24|uint32_t(ip[1])<<16|uint32_t(ip[2])<<8|ip[3];if(db->network.contains(n))settings.start=n;else show(T::Settings,"Endereco fora da sub-rede atual.");}break;}
                case 6:routerConfig();break;
                case 7:save();break;
                case 8:if(confirm("Apagar historico, nomes, notas e conhecidos apenas desta rede/AP? Essa acao nao altera os dispositivos da rede.")){if(store->erase()){for(auto &d:db->devices)d=Device{};db->count=0;db->dirty=false;db->full=false;storageValid=true;scan.reset();hasScan=false;}else show(T::Settings,"Nao foi possivel apagar o historico.");}break;
            }
        }
    }
public:
    bool start(const Network &network){
        db.reset(new(std::nothrow) Database);if(!db)return false;db->network=network;
        FS &fs=sdcardMounted?static_cast<FS &>(SD):static_cast<FS &>(LittleFS);
        store.reset(new(std::nothrow) DeviceStore(fs,network));if(!store)return false;
        storageValid=store->load(*db);routerIp=network.gateway;
        if(!storageValid)show(T::History,"Historico invalido ou armazenamento indisponivel. A base existente fica protegida. Consulte Configuracoes antes de salvar uma nova base.");
        return true;
    }
    void run(){
        networkInfo();
        while(!returnToMenu){
            int choice=choose(T::Title,{T::Scan,T::Found,T::Known,T::New,T::Details,T::Manage,T::History,T::Settings,"Atualizar scan","Resultado / alteracoes","? Ajuda"});
            if(choice<0){if(db->dirty&&storageValid){save();if(db->dirty&&!confirm("Sair sem salvar as alteracoes pendentes do historico?"))continue;}break;}
            switch(choice){case 0:case 8:runScan();break;case 1:case 4:list(0);break;case 2:list(1);break;case 3:list(2);break;case 5:list(0,true);break;case 6:list(3);break;case 7:configure();break;case 9:summary();break;case 10:MaliWiki::open(MaliWiki::Category::WIFI_INSPECTOR);break;}
        }
        router.logout();
    }
};
}
void open(){
    if(!WiFi.isConnected()&&!wifiConnectMenu())return;
    Network network;if(!currentNetwork(network)){show("Rede indisponivel","Conecte o MaliOS como cliente Wi-Fi. O Inspector requer IPv4 e mascara valida entre /1 e /30; modo AP isolado nao fornece inventario do roteador.");return;}
    Session session;if(!session.start(network)){show(T::Title,"Memoria insuficiente. Feche outras ferramentas e tente novamente.");return;}session.run();
}
}
