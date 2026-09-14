#include "MaliWiki.h"

#include "core/display.h"
#include "core/ui/MaliInput.h"
#include "core/ui/KeysPtBr.h"
#include <globals.h>

#include <Arduino.h>
#include <pgmspace.h>

namespace {
using MaliWiki::Category;

struct WikiEntry {
    Category category;
    const char *name;
    const char *what;
    const char *purpose;
    const char *example;
    const char *hardware;
    const char *warning;
};

constexpr char NO_WARNING[] = "";
constexpr char WARN_AUTH_NETWORK[] =
    "Use somente em redes e equipamentos proprios ou com autorizacao explicita. Pode afetar outros usuarios.";
constexpr char WARN_RADIO[] =
    "Transmissoes podem interferir em aparelhos proximos. Use apenas em laboratorio autorizado e respeite as regras locais.";
constexpr char WARN_CREDENTIALS[] =
    "Use apenas para treinamento autorizado. Nao colete senhas ou dados de terceiros.";
constexpr char WARN_HID[] =
    "Scripts e comandos HID controlam o equipamento conectado. Teste somente em dispositivos proprios e revise o conteudo antes.";
constexpr char WARN_STORAGE[] =
    "Interromper uma gravacao ou remover o armazenamento durante uso pode corromper arquivos.";

#define WIKI_ENTRY(category_, name_, what_, purpose_, example_, hardware_, warning_) \
    {Category::category_, name_, what_, purpose_, example_, hardware_, warning_}

// Keep entries grouped by category. Adding one row is enough to publish a new page;
// the list and scrolling UI are generated from this table.
const WikiEntry wikiEntries[] PROGMEM = {
    // Rede / Wi-Fi
    WIKI_ENTRY(
        WIFI,
        "Conectar ao Wi-Fi",
        "Abre a selecao de redes e conecta o MaliOS como estacao Wi-Fi.",
        "Permite usar ferramentas que dependem da rede local ou da Internet.",
        "Conectar ao roteador do seu laboratorio antes de abrir a WebUI.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        WIFI,
        "Iniciar AP Wi-Fi",
        "Cria o ponto de acesso configurado no MaliOS para ate quatro clientes.",
        "Fornece uma rede local quando nao ha roteador disponivel.",
        "Conectar um computador proprio ao AP para usar um servico local.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        "O AP fica visivel nas proximidades. Use senha forte e desligue-o ao terminar."
    ),
    WIKI_ENTRY(
        WIFI,
        "Desligar Wi-Fi",
        "Encerra os modos estacao e ponto de acesso e desliga o radio Wi-Fi.",
        "Libera recursos e encerra conexoes de rede ativas.",
        "Desligar o radio depois de terminar uma transferencia.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        WIFI,
        "Info do AP",
        "Mostra SSID, senha salva, RSSI, IP, gateway, canal, BSSID e seguranca da conexao.",
        "Ajuda a conferir a rede e os parametros recebidos pelo MaliOS.",
        "Confirmar o IP antes de acessar a WebUI.",
        "Wi-Fi do ESP32-S3 e tela.",
        "A senha salva pode aparecer na tela. Evite exibi-la diante de terceiros."
    ),
    WIKI_ENTRY(
        WIFI,
        "WebUI",
        "Inicia a interface web do MaliOS na rede atual ou em modo AP.",
        "Gerencia arquivos do SD e LittleFS e recursos mantidos pela interface original.",
        "Abrir o IP mostrado ou bruce.local em um navegador da mesma rede.",
        "Wi-Fi ESP32-S3, LittleFS e microSD quando presente.",
        "Preserve a autenticacao configurada e nao exponha a interface a redes nao confiaveis."
    ),
    WIKI_ENTRY(
        WIFI,
        "Ataques Wi-Fi",
        "Abre a suite existente de testes de alvo, Karma, beacon e deautenticacao.",
        "Avalia o comportamento defensivo de uma rede sem fio controlada.",
        "Executar um ensaio documentado em um AP isolado do laboratorio.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Ataques a alvo",
        "Varre APs e oferece informacoes, deauth, captura de handshake e clonagem de portal para o alvo escolhido.",
        "Agrupa testes ativos dirigidos a um unico AP de laboratorio.",
        "Validar alertas do seu roteador de teste em uma bancada isolada.",
        "Wi-Fi ESP32-S3; SD ou LittleFS para capturas e portal.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Ataque Karma",
        "Observa probe requests e pode anunciar SSIDs solicitados, iniciar portal e enviar deauth conforme a configuracao.",
        "Testa se clientes proprios tentam se associar a redes lembradas sem validacao adequada.",
        "Auditar um aparelho de teste com perfis Wi-Fi preparados para o laboratorio.",
        "Wi-Fi ESP32-S3; SD ou LittleFS quando usa portal.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Spam de beacon",
        "Transmite anuncios de AP com listas, nomes aleatorios, um nome ou nomes personalizados.",
        "Testa visualizacao e filtragem de muitos SSIDs em equipamento proprio.",
        "Conferir a interface de um scanner Wi-Fi em ambiente blindado.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Inundacao de deauth",
        "Percorre APs detectados e transmite quadros de deautenticacao repetidamente.",
        "Testa protecoes e monitoramento contra desconexoes forjadas.",
        "Validar PMF em uma rede de laboratorio sem outros usuarios.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Deauth avancado",
        "Oferece deauth individual, contra todos os APs vistos ou contra uma lista definida.",
        "Permite ensaios controlados de resiliencia e deteccao.",
        "Testar um unico AP proprio e observar o alarme do monitor.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Mali Portal",
        "Inicia o portal cativo de laboratorio com o modelo seguro selecionado no Portal Studio da WebUI.",
        "Permite demonstrar navegacao cativa com dados ficticios, sem depender de servicos externos.",
        "Abrir o Mali Lab em aparelhos proprios de uma bancada de treinamento.",
        "Wi-Fi ESP32-S3 e LittleFS para os modelos.",
        "Use somente em aparelhos e redes proprios ou em laboratorio autorizado. O Mali Lab nao solicita senhas reais."
    ),
    WIKI_ENTRY(
        WIFI,
        "Evil Portal",
        "Cria AP e portal cativo DNS/HTTP com pagina padrao ou HTML do armazenamento e registra envios em CSV.",
        "Demonstra riscos de portais falsos em treinamento autorizado.",
        "Usar dados ficticios em uma campanha interna de conscientizacao aprovada.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        WARN_CREDENTIALS
    ),
    WIKI_ENTRY(
        WIFI,
        "NetCut",
        "Descobre hosts por ARP e oferece corte, restauracao e teste por envenenamento ARP.",
        "Avalia segmentacao e deteccao de manipulacao ARP em uma LAN controlada.",
        "Testar a recuperacao de dois hosts proprios numa rede isolada.",
        "Wi-Fi ESP32-S3 e LittleFS para a lista VIP.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Escutar TCP",
        "Abre um servidor TCP na porta escolhida e troca texto com um cliente.",
        "Testa comunicacao TCP simples na rede local.",
        "Receber uma mensagem de um computador do laboratorio.",
        "Wi-Fi integrado do ESP32-S3.",
        "Abra portas somente em rede confiavel e encerre o servidor ao terminar."
    ),
    WIKI_ENTRY(
        WIFI,
        "Cliente TCP",
        "Abre uma conexao TCP bruta para o IP e a porta informados e troca texto.",
        "Ajuda a testar um servico TCP simples sob seu controle.",
        "Enviar uma linha a um servidor de eco local.",
        "Wi-Fi integrado do ESP32-S3.",
        "Conecte somente a servicos proprios ou autorizados."
    ),
    WIKI_ENTRY(
        WIFI,
        "Proxy SOCKS4",
        "Inicia um proxy SOCKS4 e SOCKS4a TCP na porta 1080, sem autenticacao implementada.",
        "Encaminha trafego TCP de um cliente da rede por meio do MaliOS.",
        "Testar um cliente SOCKS em uma LAN isolada.",
        "Wi-Fi integrado do ESP32-S3.",
        "Nao exponha o proxy a clientes nao confiaveis; ele nao exige login."
    ),
    WIKI_ENTRY(
        WIFI,
        "TelNET",
        "Cliente Telnet interativo para host e porta escolhidos, com log opcional.",
        "Administra ou testa um servico Telnet legado autorizado.",
        "Acessar um equipamento de bancada com credenciais de teste.",
        "Wi-Fi integrado do ESP32-S3; armazenamento quando o log esta ativo.",
        "Telnet nao cifra senha nem comandos. Use apenas servidor autorizado e rede protegida."
    ),
    WIKI_ENTRY(
        WIFI,
        "SSH",
        "Cliente SSH interativo que solicita host, porta, usuario e senha e roda em tarefa propria.",
        "Permite administrar um servidor autorizado pelo T-Embed.",
        "Consultar um servidor Linux do laboratorio.",
        "Wi-Fi integrado do ESP32-S3; armazenamento se o log estiver ativo.",
        "Use apenas contas e servidores autorizados e proteja os registros de sessao."
    ),
    WIKI_ENTRY(
        WIFI,
        "Sniffer",
        "Captura quadros Wi-Fi em modo promiscuo: PCAP completo, EAPOL ou deauth; um modo pode transmitir deauth.",
        "Registra trafego para diagnostico e estudo de uma rede controlada.",
        "Capturar o handshake do seu AP de laboratorio para validar o monitoramento.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Analisar canal",
        "Estima a ocupacao dos canais 1 a 11 usando quadros recebidos, RSSI e tempo de amostragem.",
        "Ajuda a comparar atividade entre canais Wi-Fi; nao e analisador de espectro RF.",
        "Escolher um canal menos ocupado para o AP do laboratorio.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        "Analise passiva; identificadores observados ainda devem ser tratados com privacidade."
    ),
    WIKI_ENTRY(
        WIFI,
        "Detectar jammer",
        "Conta quadros deauth e disassoc nos canais 1 a 11 e compara com um limite.",
        "Indica possivel abuso desses quadros; nao detecta toda interferencia de radio.",
        "Validar um alerta com trafego gerado no proprio laboratorio.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        "Funcao passiva; uma indicacao nao prova, sozinha, a existencia de jammer."
    ),
    WIKI_ENTRY(
        WIFI,
        "Procurar hosts",
        "Envia consultas ARP pela sub-rede e lista IP e MAC dos hosts encontrados.",
        "Inventaria dispositivos de uma LAN autorizada e abre acoes por host.",
        "Conferir quais placas de teste estao ligadas na sua bancada.",
        "Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Info do host",
        "Mostra fabricante do MAC e tenta uma lista fixa de portas TCP com timeout curto.",
        "Ajuda a reconhecer um host encontrado pela busca ARP.",
        "Verificar os servicos expostos por um servidor proprio.",
        "Wi-Fi ESP32-S3; W5500 quando aberto pela busca Ethernet.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "SSH do host",
        "Abre o cliente SSH para o host selecionado na varredura.",
        "Facilita o acesso autorizado a um servidor encontrado.",
        "Entrar no servidor de testes com sua propria conta.",
        "Wi-Fi integrado do ESP32-S3.",
        "Use somente servidor e credenciais autorizados."
    ),
    WIKI_ENTRY(
        WIFI,
        "Desautenticar estacao",
        "Transmite quadros Wi-Fi de deauth ou disassoc para a estacao escolhida.",
        "Testa protecao e alerta de uma rede controlada.",
        "Validar PMF entre seu AP e seu cliente de laboratorio.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Falsificacao ARP",
        "Envia respostas ARP falsas entre alvo e gateway e tenta restaurar a tabela ao parar.",
        "Avalia deteccao e isolamento contra ARP spoofing.",
        "Testar dois hosts proprios em uma LAN fisicamente isolada.",
        "Wi-Fi ou W5500 conforme a origem; armazenamento para PCAP.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Envenenamento ARP",
        "Anuncia mapeamentos ARP e MAC aleatorios para os hosts encontrados e pode registrar PCAP.",
        "Submete uma LAN de laboratorio a uma condicao agressiva de tabela ARP.",
        "Validar protecoes de um switch e hosts descartaveis isolados.",
        "W5500 via SPI e microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Esgotamento DHCP",
        "Envia DHCP Discover com enderecos MAC aleatorios ate o usuario parar.",
        "Testa protecoes do servidor DHCP contra consumo do pool.",
        "Usar um servidor DHCP descartavel numa LAN isolada.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Inundacao MAC",
        "Envia quadros com MAC e IP aleatorios para pressionar a tabela CAM do switch.",
        "Testa limites e alertas de um switch controlado.",
        "Executar somente num switch de bancada sem usuarios.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Wireguard",
        "Le o arquivo /wg.conf do microSD, sincroniza a hora e inicia um tunel WireGuard.",
        "Conecta o MaliOS a uma rede privada configurada pelo usuario.",
        "Acessar um servico do seu laboratorio pela VPN.",
        "Wi-Fi ESP32-S3 e microSD.",
        "A configuracao e as chaves podem aparecer na Serial. Trate o log como secreto."
    ),
    WIKI_ENTRY(
        WIFI,
        "Responder",
        "Responde a NBNS e LLMNR, oferece SMB e registra respostas NTLMv2 no armazenamento.",
        "Demonstra riscos de resolucao de nomes e autenticacao em rede Windows controlada.",
        "Validar endurecimento de uma maquina descartavel em laboratorio isolado.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        WARN_CREDENTIALS
    ),
    WIKI_ENTRY(
        WIFI,
        "Brucegotchi",
        "Descobre APs, interage com deauth, aguarda handshakes e anuncia beacons pwngrid em uma maquina de estados.",
        "Automatiza experimentos de monitoramento Wi-Fi em laboratorio.",
        "Observar a deteccao do seu AP de teste em ambiente isolado.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Recuperar senha",
        "Testa offline palavras de uma wordlist contra um handshake salvo em PCAP.",
        "Verifica a resistencia de uma senha de rede cuja captura e autorizada.",
        "Auditar o PCAP do seu AP usando uma lista de senhas de teste.",
        "CPU ESP32-S3, microSD ou LittleFS; nao transmite radio durante o teste.",
        WARN_CREDENTIALS
    ),

    // Bluetooth Low Energy
    WIKI_ENTRY(
        BLE,
        "Comandos de midia",
        "Emula um controle HID BLE com play, pausa, faixas, volume, mute e captura de tela.",
        "Controla midia de um host pareado conscientemente.",
        "Pausar uma apresentacao de audio no seu computador.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Use somente dispositivo proprio e pareado."
    ),
    WIKI_ENTRY(
        BLE,
        "Procurar BLE",
        "Faz scan ativo por cinco segundos e lista nome, endereco e RSSI de ate cem dispositivos.",
        "Ajuda a localizar e identificar perifericos BLE proximos.",
        "Confirmar se o seu sensor BLE esta anunciando.",
        "Bluetooth LE integrado do ESP32-S3.",
        "A busca e passiva para o usuario, mas respeite a privacidade dos identificadores vistos."
    ),
    WIKI_ENTRY(
        BLE,
        "iBeacon",
        "Anuncia continuamente um iBeacon de demonstracao com UUID fixo e MAC aleatorio.",
        "Testa aplicativos e scanners que reconhecem beacons BLE.",
        "Verificar a deteccao pelo seu telefone de laboratorio.",
        "Bluetooth LE integrado do ESP32-S3.",
        "O anuncio fica visivel nas proximidades; use em ambiente autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Bad BLE",
        "Carrega DuckyScript do SD ou LittleFS, apresenta teclado BLE e executa o script apos pareamento.",
        "Automatiza entradas de teclado em um host de teste.",
        "Executar um script inofensivo no seu computador de bancada.",
        "BLE ESP32-S3, microSD ou LittleFS.",
        WARN_HID
    ),
    WIKI_ENTRY(
        BLE,
        "Teclado BLE",
        "Oferece teclado HID BLE interativo, teclas especiais e fila de comandos pelo encoder.",
        "Digita e controla um host pareado sem teclado fisico.",
        "Escrever uma nota no seu tablet pareado.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Use somente host proprio e confira o campo que recebera as teclas."
    ),
    WIKI_ENTRY(
        BLE,
        "Spam BLE",
        "Transmite repetidamente anuncios de varios ecossistemas, com intervalo, potencia e MAC configuraveis.",
        "Testa filtragem e comportamento de interfaces diante de muitos anuncios BLE.",
        "Avaliar um telefone descartavel em bancada blindada.",
        "Bluetooth LE ESP32-S3 e NVS para configuracao.",
        "Pode gerar pop-ups e perturbar dispositivos proximos. Use apenas em laboratorio autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Suite BLE",
        "Reune perfil, conexoes, writes, fuzzing, HID, FastPair, HFP, testes de indisponibilidade e captura.",
        "Centraliza ensaios experimentais de seguranca BLE em alvos controlados.",
        "Criar um plano de teste para um periferico proprio e executar um modulo por vez.",
        "BLE ESP32-S3; nRF24 em uma rotina; armazenamento para captura.",
        "Os resultados sao experimentais e as rotinas podem travar ou alterar o alvo. Somente laboratorio autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Busca de vulnerabilidades",
        "Seleciona um alvo e combina verificacoes experimentais relacionadas a HFP e FastPair.",
        "Faz uma triagem inicial, sem garantir que um resultado seja uma vulnerabilidade real.",
        "Comparar o resultado com os logs de um dispositivo BLE proprio.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Perfil detalhado",
        "Conecta ao dispositivo e descobre servicos, caracteristicas e possibilidades de escrita.",
        "Mapeia a interface GATT de um periferico autorizado.",
        "Conferir os servicos publicados pelo seu prototipo BLE.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Conecte somente a dispositivos proprios ou autorizados."
    ),
    WIKI_ENTRY(
        BLE,
        "Ataques FastPair",
        "Agrupa testes experimentais de estado, handshake, popup, conexao e memoria ligados ao Fast Pair.",
        "Avalia como um dispositivo de laboratorio reage a entradas inesperadas.",
        "Reproduzir um caso de teste no seu acessorio Fast Pair em desenvolvimento.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Ataques HFP",
        "Agrupa testes de conexao Hands-Free, cadeia HFP e transicao experimental HFP para HID.",
        "Avalia a robustez de perfis de telefonia em equipamento controlado.",
        "Testar um headset de desenvolvimento sem outros aparelhos proximos.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Ferramentas de audio",
        "Reune testes AVRCP, alerta de telefonia e rotinas experimentais de falha de audio.",
        "Observa a resposta de um acessorio de audio proprio a comandos inesperados.",
        "Validar recuperacao de um prototipo de caixa de som.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Ataques HID",
        "Reune conexao, envio de teclas, DuckyScript e rotinas HID experimentais por sistema.",
        "Testa controles de pareamento e entrada de um host de laboratorio.",
        "Executar uma sequencia inofensiva em um computador descartavel proprio.",
        "Bluetooth LE ESP32-S3; armazenamento para scripts.",
        WARN_HID
    ),
    WIKI_ENTRY(
        BLE,
        "Corrupcao de memoria",
        "Executa testes FastPair rotulados como memoria, estado e handshake com entradas anormais.",
        "Procura falhas de robustez em um dispositivo BLE de desenvolvimento.",
        "Observar reinicio e logs do seu prototipo durante um caso controlado.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Ataques DoS",
        "Inclui flood de conexao, spam de anuncios, Jam & Connect com nRF24 e fuzzer.",
        "Testa disponibilidade e recuperacao de um alvo de laboratorio.",
        "Medir se um prototipo proprio volta a anunciar apos o teste.",
        "BLE ESP32-S3 e nRF24 externo em uma das rotinas.",
        "Pode indisponibilizar o alvo e interferir em 2,4 GHz. Use somente laboratorio isolado autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Entrega de payload",
        "Agrupa DuckyScript e tentativas experimentais relacionadas a PIN e autenticacao.",
        "Avalia controles de entrada e autorizacao de um host BLE proprio.",
        "Testar um fluxo preparado em uma maquina descartavel do laboratorio.",
        "Bluetooth LE ESP32-S3 e armazenamento para scripts.",
        WARN_HID
    ),
    WIKI_ENTRY(
        BLE,
        "Ferramentas de teste",
        "Reune descoberta de escrita, teste de audio, fuzzer e teste HID.",
        "Apoia diagnostico de um periferico BLE em desenvolvimento.",
        "Comparar servicos GATT e logs do seu prototipo.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Fuzzing pode travar o alvo; use apenas dispositivo proprio e reinicializavel."
    ),
    WIKI_ENTRY(
        BLE,
        "Cadeia universal",
        "Encadeia rotinas experimentais HFP, HID e FastPair contra o alvo selecionado.",
        "Executa uma avaliacao ampla de robustez em ambiente controlado.",
        "Usar no final de um ensaio do seu prototipo, com logs e recuperacao preparados.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Captura BLE",
        "Faz scan ativo por dez segundos, guarda anuncios, nome, MAC, RSSI e payload e permite salvar.",
        "Registra publicidade BLE para diagnostico; nao captura todos os pacotes brutos.",
        "Salvar os anuncios do seu sensor para comparar versoes de firmware.",
        "BLE ESP32-S3, microSD ou LittleFS.",
        "Identificadores podem ser pessoais. Capture somente no ambiente autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Ninebot",
        "Conecta a um dispositivo com servico Nordic UART e envia um payload fixo escolhido por modelo.",
        "Ferramenta experimental para testar um patinete proprio; o efeito exato nao e documentado no codigo.",
        "Observar logs de um controlador de bancada compativel.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Use somente no proprio equipamento e esteja preparado para desliga-lo com seguranca."
    ),
    WIKI_ENTRY(
        BLE,
        "Apresentador",
        "Emula controle HID BLE de slides com setas, contador e cronometro.",
        "Avanca ou retorna slides em um computador pareado.",
        "Controlar sua propria apresentacao.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Pareie somente com o computador que deve receber os comandos."
    ),

    // RF / Sub-GHz
    WIKI_ENTRY(
        SUB_GHZ,
        "Procurar/copiar",
        "Recebe sinais na faixa configurada, tenta decodificar ou guardar RAW e oferece repetir e salvar.",
        "Analisa e reproduz um controle ou sensor compativel sob seu controle.",
        "Testar um controle remoto proprio numa bancada.",
        "CC1101, ESP32 RMT, microSD ou LittleFS ao salvar.",
        WARN_RADIO
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Gravar RAW",
        "Grava os tempos HIGH e LOW brutos e permite repetir, salvar ou descartar a captura.",
        "Preserva sinais que nao foram reconhecidos por um decodificador.",
        "Estudar o formato de um sensor 433 MHz proprio.",
        "CC1101, ESP32 RMT, microSD ou LittleFS.",
        WARN_RADIO
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "SubGHz personalizado",
        "Carrega arquivos .sub de Recentes, SD ou LittleFS e transmite o sinal escolhido.",
        "Reproduz capturas compativeis ja armazenadas.",
        "Repetir o sinal de um transmissor proprio no laboratorio.",
        "CC1101, microSD ou LittleFS.",
        WARN_RADIO
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Espectro",
        "Desenha a atividade e a forma dos pulsos recebidos na frequencia selecionada.",
        "Ajuda a observar modulacao temporal; nao e uma FFT de toda a banda.",
        "Comparar dois controles proprios na mesma frequencia.",
        "CC1101 GDO0 e ESP32 RMT.",
        "Recepcao passiva; respeite a privacidade de sinais proximos."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Espectro RSSI",
        "Mede RSSI ao longo do tempo ou compara frequencias dentro da faixa configurada.",
        "Mostra onde ha maior energia recebida pelo CC1101.",
        "Comparar o nivel de um transmissor proprio a duas distancias.",
        "CC1101 integrado.",
        "Medicao passiva; o RSSI nao identifica sozinho a origem do sinal."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Espectro quadrado",
        "Captura os tempos HIGH e LOW e desenha a forma quadrada dos pulsos.",
        "Facilita a inspecao visual do ritmo de um sinal digital recebido.",
        "Ver a sequencia de pulsos do seu sensor de bancada.",
        "CC1101 GDO0 e ESP32 RMT.",
        "Recepcao passiva; use somente sinais que voce pode analisar."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Espectrograma",
        "Cria um waterfall de RSSI entre limites de frequencia ajustaveis.",
        "Mostra como a energia recebida varia por frequencia e tempo.",
        "Observar quando seu transmissor de teste entra no ar.",
        "CC1101 integrado.",
        "Recepcao passiva; resultados dependem da largura e sensibilidade do CC1101."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Escutar",
        "Detecta pulsos entre 300 e 928 MHz e gera bipes correspondentes; nao demodula voz ou audio.",
        "Fornece retorno sonoro da atividade de um transmissor simples.",
        "Ouvir os pulsos do seu controle remoto de bancada.",
        "CC1101 e speaker NS4168.",
        "Recepcao passiva. Nao interprete o resultado como audio do transmissor."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Forca bruta",
        "Enumera codigos de protocolos suportados, com frequencia e repeticoes configuraveis.",
        "Testa um receptor proprio contra todas as combinacoes de um protocolo simples.",
        "Avaliar o bloqueio do receptor descartavel de uma bancada isolada.",
        "CC1101 ou transmissor RF configurado.",
        "Pode acionar dispositivos. Uso estrito em receptor proprio e laboratorio autorizado."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Jammer",
        "Emite interferencia nos modos potencia total, intermitente, ruido ou varredura.",
        "Testa recuperacao de um receptor em ambiente RF controlado.",
        "Usar carga ficticia ou recinto blindado com equipamento proprio.",
        "CC1101 integrado.",
        "Pode interferir em servicos proximos e ser ilegal no ar. Somente bancada blindada autorizada."
    ),

    // nRF24
    WIKI_ENTRY(
        NRF24,
        "Informacoes",
        "Mostra orientacoes eticas e de cabeamento para o modulo nRF24.",
        "Lembra que fios longos e ruido podem causar falhas; nao detecta eletricamente o modulo.",
        "Revisar a ligacao antes de iniciar um teste.",
        "Tela; nRF24L01+ externo nas demais ferramentas.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        NRF24,
        "Espectro",
        "Le o detector RPD nos canais 0 a 79 e desenha atividade aproximada de 2,40 a 2,48 GHz.",
        "Compara ocupacao de canais; nao identifica protocolos ou dispositivos.",
        "Escolher um canal menos ocupado para duas placas proprias.",
        "nRF24L01+ externo via SPI.",
        "Medicao passiva; o resultado e apenas presenca de energia."
    ),
    WIKI_ENTRY(
        NRF24,
        "MouseJack",
        "Busca receptores Microsoft ou Logitech compativeis e oferece texto ou DuckyScript quando encontra alvo.",
        "Avalia se um receptor proprio e vulneravel a injecao sem fio conhecida como MouseJack.",
        "Testar um dongle de laboratorio e depois atualizar ou substituir o dispositivo.",
        "nRF24L01+ externo; microSD ou LittleFS para scripts.",
        "Funcao ofensiva. Use somente receptor proprio e computador de teste autorizado."
    ),
    WIKI_ENTRY(
        NRF24,
        "Jammer NRF",
        "Emite portadora e alterna grupos de canais de 2,4 GHz, de forma sequencial ou aleatoria.",
        "Testa tolerancia a interferencia em equipamento controlado.",
        "Usar dois modulos proprios dentro de um recinto RF blindado.",
        "nRF24L01+ externo via SPI.",
        "Interfere em Wi-Fi, BLE e outros sistemas de 2,4 GHz. Somente laboratorio blindado autorizado."
    ),

    // LoRa
    WIKI_ENTRY(
        LORA,
        "Chat",
        "Envia e recebe texto por SX1276 ou SX1262 e mantem historico em /chats.txt.",
        "Permite conversa simples entre radios LoRa configurados na mesma frequencia.",
        "Trocar mensagens entre duas placas proprias no laboratorio.",
        "Modulo LoRa externo via SPI e LittleFS; nao ha LoRa onboard neste alvo.",
        "Nao ha criptografia visivel. Use frequencia e potencia permitidas na sua regiao."
    ),

    // FM / SI4713
    WIKI_ENTRY(
        FM_RADIO,
        "Transmitir padrao",
        "Seleciona frequencia FM padrao e transmite RDS BruceRadio com a potencia configurada.",
        "Testa recepcao FM e RDS em bancada; esta funcao nao envia audio ao vivo.",
        "Confirmar o RDS num receptor proprio ligado a carga controlada.",
        "Transmissor SI4713 externo via I2C.",
        "Transmissao FM e regulada. Use carga ficticia, blindagem ou autorizacao."
    ),
    WIKI_ENTRY(
        FM_RADIO,
        "Transmitir reservado",
        "Permite transmissao RDS na faixa aproximada de 76 a 86,9 MHz.",
        "Oferece teste de receptor em uma faixa que pode ser reservada conforme a regiao.",
        "Ensaiar um receptor de bancada dentro de recinto blindado.",
        "Transmissor SI4713 externo via I2C.",
        "A faixa pode ser restrita ou ilegal. Nao transmita por antena fora de laboratorio autorizado."
    ),
    WIKI_ENTRY(
        FM_RADIO,
        "Parar transmissao",
        "Zera a potencia, reseta o SI4713 e limpa o estado de transmissao.",
        "Encerra com seguranca uma sessao de teste FM.",
        "Parar o transmissor antes de desconectar o modulo.",
        "Transmissor SI4713 externo via I2C.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        FM_RADIO,
        "Espectro FM",
        "Mede e desenha o nivel de ruido retornado pelo SI4713 na frequencia escolhida.",
        "Ajuda a comparar uma frequencia; nao e waterfall de toda a banda.",
        "Comparar duas frequencias antes de um ensaio blindado.",
        "SI4713 externo via I2C.",
        "Medicao passiva. O valor nao identifica a estacao ou o conteudo."
    ),
    WIKI_ENTRY(
        FM_RADIO,
        "Sequestrar TA",
        "Transmite RDS BruceTraffic em 107,7 MHz com o bit Traffic Announcement ativo.",
        "Demonstra como receptores proprios reagem a um anuncio de transito RDS.",
        "Testar um radio descartavel dentro de caixa RF blindada.",
        "Transmissor SI4713 externo via I2C.",
        "Alto risco de interferencia. Use somente carga ficticia ou recinto totalmente blindado autorizado."
    ),

    // Infravermelho
    WIKI_ENTRY(
        IR,
        "TV-B-Gone",
        "Envia uma colecao de comandos de energia para regioes NA ou EU e codigos universais.",
        "Verifica quais codigos de energia sao aceitos por uma TV propria.",
        "Testar a compatibilidade da sua TV na bancada.",
        "LED IR transmissor no GPIO configurado.",
        "Pode desligar aparelhos proximos. Use somente equipamentos proprios ou autorizados."
    ),
    WIKI_ENTRY(
        IR,
        "IR personalizado",
        "Abre arquivos .ir de Recentes, SD ou LittleFS e envia um comando ou todos.",
        "Reproduz comandos infravermelhos previamente salvos.",
        "Acionar o seu ar-condicionado com um arquivo proprio.",
        "LED IR transmissor, microSD ou LittleFS.",
        "Nao controle aparelhos de terceiros sem permissao."
    ),
    WIKI_ENTRY(
        IR,
        "Ler IR",
        "Captura IR decodificado ou RAW, permite retransmitir, salvar e criar controles rapidos.",
        "Aprende comandos de um controle remoto proprio.",
        "Capturar e repetir o volume do seu televisor.",
        "Receptor IR, LED IR transmissor, microSD ou LittleFS.",
        "A leitura e passiva, mas a repeticao transmite comandos. Use aparelhos proprios."
    ),
    WIKI_ENTRY(
        IR,
        "Jammer IR",
        "Emite padroes Basic, Enhanced, Sweep, Random ou Empty em portadora ajustavel.",
        "Testa como um receptor IR proprio se recupera de sinais concorrentes.",
        "Avaliar seu receptor dentro de uma bancada sem outros aparelhos.",
        "LED IR transmissor.",
        "Pode bloquear controles proximos. Use somente ambiente controlado autorizado."
    ),

    // Ethernet
    WIKI_ENTRY(
        ETHERNET,
        "Procurar hosts",
        "Inicializa o W5500, percorre a sub-rede com ARP e lista IP, MAC e gateway.",
        "Inventaria uma rede Ethernet autorizada.",
        "Localizar placas proprias conectadas ao switch da bancada.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Info do host",
        "Mostra fabricante do MAC e tenta uma lista fixa de portas TCP no host selecionado.",
        "Ajuda a reconhecer e diagnosticar um equipamento encontrado.",
        "Conferir servicos do seu servidor de testes.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Conectar via SSH",
        "Abre o cliente SSH para o host, mas esta acao usa Wi-Fi, mesmo iniciada na lista Ethernet.",
        "Permite uma sessao autorizada no host encontrado.",
        "Entrar no servidor proprio com uma conta de teste.",
        "Wi-Fi ESP32-S3.",
        "Use somente servidor e credenciais autorizados."
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Desautenticar estacao",
        "Usa o radio Wi-Fi para enviar deauth ao host quando a conexao Wi-Fi esta disponivel.",
        "Testa protecao Wi-Fi de uma estacao propria encontrada.",
        "Validar PMF em cliente e AP de laboratorio.",
        "Wi-Fi ESP32-S3; nao usa os quadros Ethernet do W5500.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Falsificacao ARP",
        "Envia respostas ARP falsas entre alvo e gateway, grava PCAP e tenta restaurar a tabela ao parar.",
        "Testa protecoes contra ARP spoofing numa LAN controlada.",
        "Observar alertas de dois hosts proprios em switch isolado.",
        "W5500 externo, microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Envenenamento ARP",
        "Percorre hosts e anuncia mapeamentos ARP e MAC aleatorios, com registro PCAP.",
        "Submete uma rede descartavel a um teste agressivo de tabela ARP.",
        "Validar isolamento de um switch de laboratorio.",
        "W5500 externo, microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Esgotar DHCP",
        "Transmite DHCP Discover com MACs aleatorios ate o usuario interromper.",
        "Testa protecao contra consumo do pool DHCP.",
        "Usar um servidor DHCP descartavel numa LAN isolada.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Inundacao MAC",
        "Envia quadros com MAC e IP aleatorios para pressionar a tabela CAM do switch.",
        "Testa limites e alertas de um switch controlado.",
        "Ensaiar um switch de bancada sem outros clientes.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),

    // GPS
    WIKI_ENTRY(
        GPS,
        "Wardriving",
        "Associa scans Wi-Fi, BLE ou ambos a coordenadas GPS e grava CSV no formato WiGLE.",
        "Mapeia cobertura e inventario de radios em uma area autorizada.",
        "Levantar os APs e beacons do seu laboratorio e comparar cobertura.",
        "GPS UART externo, Wi-Fi e BLE ESP32-S3, microSD ou LittleFS.",
        "Coleta MACs e localizacao. Respeite privacidade e use apenas levantamento autorizado."
    ),
    WIKI_ENTRY(
        GPS,
        "Rastreador GPS",
        "Le coordenadas, acumula distancia e grava uma trilha GPX em /BruceGPS.",
        "Registra o percurso do proprio aparelho.",
        "Gravar uma caminhada de teste e abrir o GPX no computador.",
        "GPS UART externo, microSD ou LittleFS.",
        "O arquivo revela localizacao. Proteja-o e obtenha consentimento quando necessario."
    ),

    // NFC / RFID
    WIKI_ENTRY(
        RFID,
        "Ler tag",
        "Le tipo, UID, ATQA, SAK e dados disponiveis e oferece verificar, salvar, clonar, gravar ou emular conforme suporte.",
        "Inspeciona uma tag propria e prepara operacoes compativeis.",
        "Identificar uma tag de laboratorio e salvar seu backup.",
        "Modulo RFID selecionado; padrao PN532 I2C integrado; armazenamento.",
        "UIDs e dumps podem ser credenciais. Use somente tags proprias ou autorizadas."
    ),
    WIKI_ENTRY(
        RFID,
        "Ler EMV",
        "Consulta dados contactless disponiveis, como AID, emissor, PAN e datas, e pode salvar o resultado.",
        "Demonstra quais dados um cartao EMV proprio expoe por aproximacao.",
        "Verificar seu cartao de teste sem realizar transacao.",
        "PN532 por I2C ou SPI e armazenamento.",
        "Dados financeiros sao sensiveis. Use somente cartao proprio e com consentimento."
    ),
    WIKI_ENTRY(
        RFID,
        "Ler 125kHz",
        "Recebe pela UART um pacote de leitor 125 kHz, valida checksum e pode salvar .rfidlf.",
        "Le o identificador de uma tag LF; esta funcao nao clona nem grava.",
        "Inventariar uma chave de laboratorio conhecida.",
        "Leitor RFID 125 kHz externo via UART.",
        "Identificadores podem controlar acesso. Leia apenas tags proprias ou autorizadas."
    ),
    WIKI_ENTRY(
        RFID,
        "Ferramenta SRIX",
        "Le UID e dump de SRIX4K ou SRIX512, salva e carrega .srix e pode escrever ou clonar o dump.",
        "Faz manutencao e pesquisa de tags SRIX compativeis.",
        "Salvar backup de uma tag de laboratorio antes de testar escrita.",
        "PN532 I2C integrado; antena desta placa pode ter alcance limitado.",
        "Gravacao pode corromper a tag. Use tag propria, backup e autorizacao."
    ),
    WIKI_ENTRY(
        RFID,
        "Procurar tags",
        "Registra continuamente UIDs unicos encontrados e salva o resultado ao sair.",
        "Cria um inventario rapido de tags presentes numa bancada.",
        "Contar as etiquetas de um kit de laboratorio.",
        "Modulo RFID selecionado e armazenamento.",
        "Inventarie apenas tags proprias ou autorizadas."
    ),
    WIKI_ENTRY(
        RFID,
        "Carregar arquivo",
        "Carrega um dump RFID e oferece verificar, gravar, clonar UID ou emular conforme o driver.",
        "Restaura ou usa um backup criado pelo proprio usuario.",
        "Regravar uma tag descartavel a partir do seu backup.",
        "Modulo RFID, microSD ou LittleFS.",
        "O dump pode ser credencial e as acoes podem alterar tags. Laboratorio autorizado."
    ),
    WIKI_ENTRY(
        RFID,
        "Apagar dados",
        "Chama a operacao de apagamento do driver sobre a tag apresentada.",
        "Limpa uma tag gravavel quando o modulo e o tipo suportam a acao.",
        "Apagar uma tag descartavel depois de fazer backup.",
        "Modulo RFID selecionado.",
        "Acao destrutiva. Use somente tag propria, descartavel ou com backup."
    ),
    WIKI_ENTRY(
        RFID,
        "Emular NDEF",
        "Cria NDEF de texto, URL, Wi-Fi ou link salvo e coloca modulo compativel em modo alvo.",
        "Apresenta conteudo NDEF a um leitor proximo sem gravar uma etiqueta.",
        "Compartilhar uma URL de teste com seu telefone.",
        "PN532 ou ST25R compativel.",
        "O conteudo fica disponivel a leitores proximos. Nao use dados sensiveis."
    ),
    WIKI_ENTRY(
        RFID,
        "Gravar NDEF",
        "Cria NDEF de texto, URL, Wi-Fi ou link e tenta grava-lo em tag compativel.",
        "Prepara uma etiqueta NFC com conteudo simples.",
        "Gravar uma URL propria em uma tag de teste.",
        "Modulo RFID e tag gravavel; suporte principal a MIFARE Ultralight.",
        "Pode sobrescrever o conteudo. Use tag propria e mantenha backup."
    ),
    WIKI_ENTRY(
        RFID,
        "Amiibolink",
        "Conecta por BLE ao Amiibolink, envia dump NTAG215 valido e ajusta o modo de UID.",
        "Gerencia um acessorio Amiibolink externo com backups compativeis.",
        "Carregar no acessorio um dump legitimo criado pelo usuario.",
        "BLE ESP32-S3, Amiibolink externo e armazenamento.",
        "Use somente backups proprios e respeite licencas, regras e direitos aplicaveis."
    ),
    WIKI_ENTRY(
        RFID,
        "Chameleon",
        "Controla Chameleon Ultra por BLE para ler, buscar, salvar, gravar e emular tags HF ou LF.",
        "Centraliza operacoes de laboratorio com o periferico Chameleon.",
        "Ler e salvar uma tag de teste antes de uma alteracao.",
        "BLE ESP32-S3, Chameleon Ultra externo e armazenamento.",
        "Clonar, gravar ou emular somente tags proprias ou autorizadas. Reset de fabrica apaga dados."
    ),
    WIKI_ENTRY(
        RFID,
        "PN532 BLE",
        "Controla PN532 BLE ou PN532Killer externo para leitura, dump, escrita e emulacao compativeis.",
        "Usa um leitor NFC remoto em ensaios de bancada.",
        "Salvar o dump de uma tag propria com o leitor externo.",
        "BLE ESP32-S3 e PN532 BLE ou PN532Killer externo.",
        "Dumps, gravacao e emulacao somente em tags proprias ou autorizadas."
    ),
    WIKI_ENTRY(
        RFID,
        "PN532 UART",
        "Controla PN532 ou PN532Killer por UART; no Killer inclui emulacao, sniffers e pontes BLE/TCP/UDP.",
        "Opera e diagnostica um leitor NFC serial externo.",
        "Buscar o UID de uma tag propria pelo modulo conectado.",
        "UART, PN532 externo e opcionalmente BLE, Wi-Fi e armazenamento.",
        "Sniffing, ponte e emulacao apenas em laboratorio proprio; nao exponha a ponte a redes nao confiaveis."
    ),

    // Arquivos e compartilhamento
    WIKI_ENTRY(
        FILES,
        "Cartao SD",
        "Abre o gerenciador do microSD para navegar, ver informacoes, renomear, copiar, excluir e abrir arquivos suportados.",
        "Gerencia arquivos grandes e dados removiveis usados pelas ferramentas.",
        "Copiar uma captura propria para outra pasta antes de analisa-la.",
        "Cartao microSD e barramento SPI.",
        "Excluir e sobrescrever sao acoes reais. Ejete o cartao e mantenha backup de dados importantes."
    ),
    WIKI_ENTRY(
        FILES,
        "LittleFS",
        "Abre o mesmo gerenciador de arquivos na particao interna LittleFS.",
        "Gerencia configuracoes, scripts e capturas na flash quando nao ha microSD.",
        "Renomear um arquivo de teste salvo na memoria interna.",
        "Flash interna do ESP32-S3.",
        "O espaco e limitado. Exclusoes e sobrescritas nao sao desfeitas automaticamente."
    ),
    WIKI_ENTRY(
        FILES,
        "Enviar arquivo",
        "Descobre outro Bruce ou MaliOS e envia um arquivo em blocos por ESP-NOW.",
        "Transfere dados localmente entre dois dispositivos compativeis.",
        "Enviar uma captura de teste para outra placa propria.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        "Envie somente a peer autorizado e confira se o arquivo nao contem dados sensiveis."
    ),
    WIKI_ENTRY(
        FILES,
        "Receber arquivo",
        "Aguarda transferencia ESP-NOW e grava o arquivo no armazenamento escolhido sem substituir nome existente.",
        "Recebe dados de outro dispositivo Bruce ou MaliOS.",
        "Receber um script conhecido da sua segunda placa.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        "Aceite arquivos somente de origem confiavel e revise-os antes de executar."
    ),
    WIKI_ENTRY(
        FILES,
        "Enviar comandos",
        "Envia comandos do console Bruce para outro dispositivo por ESP-NOW.",
        "Administra uma segunda placa propria sem cabo serial.",
        "Solicitar uma informacao inofensiva a uma placa de bancada.",
        "Wi-Fi integrado do ESP32-S3.",
        "Comandos podem alterar o aparelho remoto. Use somente peer proprio e autorizado."
    ),
    WIKI_ENTRY(
        FILES,
        "Receber comandos",
        "Aguarda comandos ESP-NOW e os entrega ao interpretador de comandos do firmware.",
        "Permite controle remoto entre dispositivos Bruce ou MaliOS confiaveis.",
        "Habilitar temporariamente em duas placas isoladas para um teste.",
        "Wi-Fi integrado do ESP32-S3.",
        "Quem envia pode acionar funcoes do aparelho. Ative somente em laboratorio e com peer confiavel."
    ),
    WIKI_ENTRY(
        FILES,
        "Armazenamento USB",
        "Usa a implementacao original do Bruce para expor os setores brutos do microSD ao computador por USB MSC.",
        "Permite copiar arquivos do cartao como em uma unidade USB; nao expoe LittleFS nesta build.",
        "Conectar ao PC, copiar um arquivo e ejetar a unidade antes de sair.",
        "USB nativa do ESP32-S3 e cartao microSD.",
        "Ejete com seguranca. Remover cabo ou cartao durante escrita pode corromper o sistema de arquivos."
    ),

    // Scripts JavaScript
    WIKI_ENTRY(
        SCRIPTS,
        "Executar script JS",
        "Lista .js e .bjs encontrados nas pastas suportadas ou permite carregar outro arquivo e roda o motor mJS.",
        "Automatiza funcoes expostas pelo interpretador do Bruce.",
        "Executar um script proprio que mostra uma mensagem na tela.",
        "ESP32-S3, microSD ou LittleFS; hardware adicional depende do script.",
        "Script e codigo: pode acessar arquivos, rede e hardware. Execute somente conteudo confiavel e revisado."
    ),

    // Relogio
    WIKI_ENTRY(
        CLOCK,
        "Relogio",
        "Mostra a hora de software atualizada a cada segundo; esta placa nao possui RTC fisico definido.",
        "Consulta a hora configurada ou sincronizada no firmware.",
        "Deixar a tela aberta como relogio de bancada.",
        "ESP32-S3 e tela.",
        "A hora pode se perder ou desviar sem sincronizacao; nao use como referencia critica."
    ),
    WIKI_ENTRY(
        CLOCK,
        "Temporizador",
        "Configura uma contagem regressiva de ate 99:59:59 e toca o som escolhido ao terminar.",
        "Lembra o fim de uma atividade simples.",
        "Marcar cinco minutos de um teste de bancada.",
        "Tela, encoder e speaker NS4168 por I2S.",
        "Nao e temporizador certificado para processos de seguranca ou tempo critico."
    ),

    // Ferramentas e aplicativos
    WIKI_ENTRY(
        OTHERS,
        "D20",
        "Simula dados D4, D6, D8, D10, D12, D20 e D100 com rolagem rapida e historico.",
        "Fornece sorteios simples para jogos e testes.",
        "Rolar um D20 durante uma partida.",
        "Gerador aleatorio do ESP32-S3, tela e encoder.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        OTHERS,
        "Mini Pixel Paint",
        "Editor monocromatico 16 por 16 que pinta, apaga, limpa, salva, carrega e exporta header.",
        "Cria pequenos sprites diretamente no dispositivo.",
        "Desenhar um icone e exportar o array .h para um projeto proprio.",
        "Tela, encoder, microSD ou LittleFS.",
        "O slot de pintura e fixo; salve uma copia antes de sobrescrever trabalho importante."
    ),
    WIKI_ENTRY(
        OTHERS,
        "QRCodes",
        "Exibe QRs salvos e cria payload PIX ou QR personalizado para mostrar, salvar ou remover.",
        "Compartilha texto ou dados codificados visualmente.",
        "Mostrar a URL do seu projeto para outro telefone.",
        "Tela, encoder e configuracao na flash.",
        "Confira o conteudo antes de compartilhar. PIX apenas monta o payload e nao confirma pagamento."
    ),
    WIKI_ENTRY(
        OTHERS,
        "Megalodon",
        "Minijogo de tubarao e peixes com pontuacao.",
        "Oferece entretenimento local no dispositivo.",
        "Jogar uma rodada usando o encoder e botoes.",
        "Tela, encoder e botoes.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        OTHERS,
        "Espectro do microfone",
        "Captura audio I2S e desenha o espectro e o historico calculados por FFT.",
        "Visualiza a distribuicao aproximada de frequencias do som ambiente.",
        "Observar a resposta a um tom gerado na bancada.",
        "Microfone SPM1423 integrado, I2S e tela.",
        "Respeite privacidade e consentimento ao captar conversas ou ambientes compartilhados."
    ),
    WIKI_ENTRY(
        OTHERS,
        "Gravar microfone",
        "Grava WAV mono de 48 kHz e 16 bits por duracao e ganho configurados; stealth reduz o brilho.",
        "Registra audio para teste e diagnostico local.",
        "Gravar alguns segundos de um alto-falante proprio para comparar qualidade.",
        "Microfone SPM1423 I2S, microSD ou LittleFS.",
        "Grave somente com consentimento. Modo stealth nao altera obrigacoes legais ou de privacidade."
    ),
    WIKI_ENTRY(
        OTHERS,
        "iButton",
        "Le UID OneWire DS1990A, valida CRC, salva ou carrega .ibtn e grava chaves RW1990 compativeis.",
        "Faz inventario, backup e teste de chaves iButton proprias.",
        "Salvar o UID de uma chave de laboratorio e testar uma RW1990 descartavel.",
        "Probe OneWire externo no GPIO configurado, microSD ou LittleFS.",
        "UID pode ser credencial de acesso. Copie somente chaves e sistemas proprios autorizados."
    ),

    // BadUSB & HID
    WIKI_ENTRY(
        USB_HID,
        "BadUSB",
        "Seleciona DuckyScript no SD ou LittleFS, cria teclado USB HID e executa o script apos confirmacao.",
        "Automatiza entradas de teclado em computador de teste.",
        "Executar um script inofensivo que abre um editor no seu PC de bancada.",
        "USB nativa ESP32-S3, microSD ou LittleFS.",
        WARN_HID
    ),
    WIKI_ENTRY(
        USB_HID,
        "Teclado USB",
        "Emula teclado USB interativo com texto, modificadores, navegacao, funcoes e fila de comandos.",
        "Permite digitar no host conectado usando a interface do T-Embed.",
        "Escrever uma mensagem curta no seu computador.",
        "USB nativa ESP32-S3, tela e encoder.",
        WARN_HID
    ),
    WIKI_ENTRY(
        USB_HID,
        "Clicador USB",
        "Emula mouse USB e gera cliques com intervalo, quantidade e botao configuraveis.",
        "Automatiza um ensaio repetitivo de interface em host proprio.",
        "Testar um botao de um aplicativo desenvolvido por voce.",
        "USB nativa do ESP32-S3.",
        "Pode produzir cliques involuntarios. Use somente aplicacao e computador proprios."
    ),
    WIKI_ENTRY(
        USB_HID,
        "USB U2F",
        "Atua como autenticador USB U2F ou CTAP, com cadastro, login e presenca confirmada pelo botao.",
        "Testa autenticacao de dois fatores compativel com o firmware.",
        "Cadastrar em uma conta de laboratorio que possui outro metodo de recuperacao.",
        "USB nativa, flash Preferences e LittleFS, tela e encoder.",
        "Valide compatibilidade e mantenha recuperacao. Nao use como unico fator em conta importante sem testes."
    ),

    // Mali Tools
    WIKI_ENTRY(
        MALI_TOOLS,
        "Info. do Sistema",
        "Mostra versoes, chip, revisao, CPU, flash, heap, PSRAM, tempo ligado, MAC e dados da compilacao.",
        "Ajuda a identificar firmware e recursos disponiveis para diagnostico.",
        "Conferir memoria livre antes de relatar um problema.",
        "ESP32-S3 e tela.",
        "O endereco MAC identifica o aparelho; evite publica-lo sem necessidade."
    ),
    WIKI_ENTRY(
        MALI_TOOLS,
        "Status do Hardware",
        "Mostra estados Wi-Fi e BLE e a configuracao de pinos de CC1101, nRF24, PN532, SD e GPS.",
        "Confere se o firmware tem recursos e pinos configurados; nao e teste eletrico garantido.",
        "Verificar a configuracao do nRF24 antes de conectar o modulo.",
        "ESP32-S3 e os perifericos listados, quando instalados.",
        "Um status configurado nao garante que o modulo fisico esteja presente ou saudavel."
    ),
    WIKI_ENTRY(
        MALI_TOOLS,
        "Energia",
        "Mostra bateria, tensao, carga e tempo ligado usando as leituras disponiveis.",
        "Ajuda a acompanhar a alimentacao durante testes.",
        "Conferir a tensao antes de iniciar uma sessao longa.",
        "Fuel gauge BQ27220 por I2C e ESP32-S3.",
        "Leituras sao indicativas; nao substituem instrumento de medicao para trabalho critico."
    ),
    WIKI_ENTRY(
        MALI_TOOLS,
        "Sobre o MaliOS",
        "Mostra versao, creditos, dados da compilacao e modelo do dispositivo.",
        "Identifica a distribuicao instalada e reconhece os projetos de origem.",
        "Consultar a versao ao abrir um relato de erro.",
        "Tela e informacoes compiladas no firmware.",
        NO_WARNING
    ),

    WIKI_ENTRY(
        MALI_TOOLS, "Chaves",
        "Catalogo de referencias de chaves planas e cruciformes, com medidas manuais em mm e quatro faces independentes.",
        "Identificar, visualizar e comparar geometrias sem calcular profundidades de corte.",
        "Abrir Ferramentas > Mali Tools > Chaves. Escolher um tipo, medir, editar dados e salvar no catalogo.",
        "Tela, encoder e SD ou LittleFS. Regua ou paquimetro externo para medir.",
        "O desenho e ilustrativo, sem escala fisica 1:1. Fabricante e um dado informado, nao uma identificacao automatica."
    ),
    WIKI_ENTRY(
        MALI_TOOLS, "KEY GAUGE",
        "Mantem o editor geometrico anterior, separado do novo catalogo Chaves e de seus registros.",
        "Consultar e editar os perfis relativos ja existentes, inclusive pela WebUI.",
        "Abrir KEY GAUGE; ajustar pontos, espessura de 1 a 10 e largura visual de 50 a 100 por cento. Salvar explicitamente.",
        "Tela e armazenamento em /MaliTools/KeyGauge. Wi-Fi para a WebUI.",
        "Espessura e largura sao escalas visuais independentes dos niveis. Previa Web usa RAM; nao grava automaticamente."
    ),
    WIKI_ENTRY(
        MALI_TOOLS, "Testes de resiliencia / Counter",
        "Abre o laboratorio Counter com configuracao, simulacao, observacao, metricas e historico conforme cada modulo.",
        "Acompanhar ensaios controlados e comparar resultados no aparelho ou na WebUI.",
        "Escolher o modulo e o modo, revisar intensidade e alvo, iniciar e usar Parar para encerrar.",
        "Wi-Fi/BLE integrados; CC1101, nRF24, IR ou NFC conforme a capacidade informada.",
        "Modos ativos exigem a confirmacao de autorizacao. A simulacao nao equivale a uma transmissao real."
    ),
    WIKI_ENTRY(
        MALI_TOOLS, "Navegacao MaliOS",
        "Organiza o menu em Rede, Radio, Ferramentas, Counter, Arquivos e Sistema com cartoes e vizinhos visiveis.",
        "Manter o acesso as ferramentas anteriores usando o encoder.",
        "Girar escolhe a opcao; clicar abre; segurar volta. Listas longas continuam compactas e rolaveis.",
        "Tela e encoder do T-Embed.",
        "Menus ocultados nas configuracoes continuam ocultos ate serem reativados."
    ),
    WIKI_ENTRY(
        MALI_TOOLS, "D20 e Pixel Paint",
        "D20 rola dados com historico. Pixel Paint edita uma grade de pixels com painel adaptado a orientacao da tela.",
        "Usar os aplicativos locais com a identidade visual MaliOS.",
        "Em D20, selecionar o dado e clicar para rolar. No Pixel Paint, abrir as acoes para trocar modo, cor ou salvar.",
        "Tela e encoder; armazenamento para desenhos salvos.", WARN_STORAGE
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Chave Plana",
        "Registra comprimento total e util, largura, espessura, posicoes aproximadas, lado, orientacao, cabeca e canaletas.",
        "Catalogar uma chave vista de lado, com fabricante informado, tipo de perfil e observacoes.",
        "Escolher Chave Plana > Medir Chave; depois Editar dados, Visualizar e Salvar. Retomar pelo item Continuar rascunho.",
        "Tela, encoder e regua ou paquimetro.",
        "A faixa hachurada marca a regiao serrilhada. Nao ha tabela de profundidades nem perfil de corte para fabricar."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Chave Cruciforme",
        "Mostra uma haste e sua secao frontal em cruz, identificando as faces A, B, C e D.",
        "Registrar por face posicoes, espacamento visual, comprimento, braco, largura, orientacao e notas.",
        "Em Visualizar, girar alterna a face destacada. Na vista frontal A fica acima, B a direita, C abaixo e D a esquerda.",
        "Tela e encoder; medicao manual de cada braco.",
        "Braco significa distancia do centro ate a borda externa. Espacamento de 50 a 150 por cento e apenas visual."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Medir Chave",
        "Recebe medidas manuais em milimetros. Zero representa uma medida ainda nao informada.",
        "Manter grandezas separadas e coerentes: comprimento util nao supera o total; comprimento da face nao supera o util.",
        "Medir com paquimetro; girar altera o valor, clicar confirma e segurar cancela. Ajustar passo em Configuracoes.",
        "Regua ou paquimetro externo. O aparelho nao mede automaticamente.",
        "A precisao exibida e de 0,01 mm; ela nao aumenta a precisao do instrumento nem torna a tela uma regua calibrada."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Catalogo: salvar e carregar",
        "Salva registros pequenos em /MaliKeys/, no SD disponivel ao entrar ou no armazenamento interno.",
        "Consultar dados depois de reiniciar, mantendo Chaves separado dos arquivos antigos .mkg.",
        "Salvar solicita nome. No Catalogo, selecionar um registro e Visualizar para carregar seus dados. Salvar como cria uma copia.",
        "SD ou LittleFS; formato .mkey com versao e verificacao de integridade.",
        "Limite de 256 registros. Nomes aceitam letras, numeros, espacos internos, - e _. Gravar usa arquivo temporario e copia de recuperacao."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Renomear e excluir",
        "O menu de cada registro oferece Renomear e Excluir. Excluir exige confirmacao e nomes existentes nao sao sobrescritos ao renomear.",
        "Organizar referencias sem alterar suas medidas ou os perfis do KEY GAUGE.",
        "Abrir Catalogo, escolher o registro, selecionar a acao e confirmar. Segurar cancela ou volta.",
        "O mesmo armazenamento escolhido ao entrar em Chaves.", WARN_STORAGE
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Comparar",
        "Abre dois registros salvos e apresenta diferencas de comprimento, largura, espessura, posicoes, cabeca e tipo.",
        "Comparar geometria informada, incluindo dimensoes de cada face quando ambos sao cruciformes.",
        "Catalogo > Comparar: escolher Perfil A e Perfil B. Os deltas usam A menos B; medidas ausentes aparecem como nao informadas.",
        "Dois registros salvos em /MaliKeys/.",
        "Semelhante indica igualdade de uma categoria informada; nao indica compatibilidade com fechaduras."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Texto, data e configuracoes",
        "O editor de texto usa o encoder para escolher caracteres, Espaco, Apagar ultimo e Concluir texto.",
        "Editar nome, fabricante, perfil, notas e data sem teclado externo.",
        "Girar escolhe, clicar insere, segurar cancela. Data aceita AAAA-MM-DD ou vazio. Passo e guias valem durante a sessao.",
        "Tela e encoder. Data inicial usa o relogio do sistema quando disponivel.",
        "Alteracoes pendentes sao sinalizadas com *. Voltar ao menu Chaves mantem o rascunho; sair solicita confirmar descarte."
    ),

    // Mali Counter
    WIKI_ENTRY(
        MALI_COUNTER, "Counter Suite",
        "Reune monitores de Wi-Fi, BLE, RF, NFC, IR e 2,4 GHz, com painel de ultimo estado e registro de eventos.",
        "Observar atividade dos receptores disponiveis e consultar resultados sem confundir eventos com dispositivos unicos.",
        "Abrir Counter Suite, escolher um monitor e girar para mudar a pagina. Em IR, clicar permite salvar; em RF, reinicia o pico.",
        "Radios integrados e perifericos conforme a configuracao e disponibilidade.",
        "A ferramenta informa quando o hardware esta indisponivel. Contagens dependem do periodo observado."
    ),
    WIKI_ENTRY(
        MALI_COUNTER, "Testes de resiliencia",
        "O item Testes de resiliencia da categoria Counter abre o laboratorio com modos, intensidade, simulacao e metricas ao vivo.",
        "Comparar ensaios controlados no dispositivo ou na WebUI.",
        "Selecionar o modulo, revisar modo e alvo, iniciar e usar Parar para encerrar. Consultar o historico ao terminar.",
        "Os modos disponiveis acompanham as capacidades do hardware.",
        "Simulacao nao transmite. Modos ativos exigem confirmar autorizacao e podem afetar o alvo de laboratorio."
    ),
    WIKI_ENTRY(
        MALI_COUNTER,
        "Painel / Varredura completa",
        "Executa ciclos controlados de varredura Wi-Fi, BLE, nRF24 e RSSI CC1101 e mostra contagens no painel.",
        "Compara atividade observada pelos radios sem tratar eventos como dispositivos unicos.",
        "Observar a mudanca do painel ao ligar um sensor proprio na bancada.",
        "Wi-Fi e BLE ESP32-S3, CC1101 integrado e nRF24 externo via SPI.",
        "Varreduras sao de observacao. Contagens sao eventos, nao dispositivos; respeite privacidade e regras locais."
    ),
};

#undef WIKI_ENTRY

constexpr size_t WIKI_ENTRY_COUNT = sizeof(wikiEntries) / sizeof(wikiEntries[0]);
constexpr int16_t LIST_TOP = 42;
constexpr int16_t LIST_ROW_HEIGHT = 16;
constexpr int16_t TOPIC_TOP = 32;
constexpr int16_t TOPIC_LINE_HEIGHT = LH + 2;
constexpr int16_t FOOTER_HEIGHT = 18;

size_t countEntries(Category category) {
    size_t count = 0;
    for (size_t i = 0; i < WIKI_ENTRY_COUNT; ++i) {
        if (wikiEntries[i].category == category) ++count;
    }
    return count;
}

const WikiEntry *entryAt(Category category, size_t ordinal) {
    for (size_t i = 0; i < WIKI_ENTRY_COUNT; ++i) {
        if (wikiEntries[i].category != category) continue;
        if (ordinal == 0) return &wikiEntries[i];
        --ordinal;
    }
    return nullptr;
}

void copyClipped(const char *source, char *output, size_t outputSize, size_t maxChars) {
    if (outputSize == 0) return;
    if (source == nullptr) {
        output[0] = '\0';
        return;
    }

    const size_t length = strlen(source);
    const size_t limit = min(maxChars, outputSize - 1);
    if (length <= limit) {
        memcpy(output, source, length + 1);
        return;
    }

    const size_t copyLength = limit > 3 ? limit - 3 : limit;
    memcpy(output, source, copyLength);
    size_t cursor = copyLength;
    while (cursor < limit) output[cursor++] = '.';
    output[limit] = '\0';
}

bool nextWrappedLine(
    const char *text, size_t &offset, char *output, size_t outputSize, size_t maxChars
) {
    if (text == nullptr || outputSize == 0 || maxChars == 0) return false;

    while (text[offset] == ' ') ++offset;
    if (text[offset] == '\0') return false;
    if (text[offset] == '\n') {
        ++offset;
        output[0] = '\0';
        return true;
    }

    const size_t start = offset;
    size_t cursor = start;
    size_t lastSpace = SIZE_MAX;
    while (text[cursor] != '\0' && text[cursor] != '\n' && cursor - start < maxChars) {
        if (text[cursor] == ' ') lastSpace = cursor;
        ++cursor;
    }

    size_t end = cursor;
    if (text[cursor] == '\n') {
        offset = cursor + 1;
    } else if (text[cursor] == '\0') {
        offset = cursor;
    } else if (lastSpace != SIZE_MAX && lastSpace > start) {
        end = lastSpace;
        offset = lastSpace + 1;
        while (text[offset] == ' ') ++offset;
    } else {
        offset = cursor;
    }

    while (end > start && text[end - 1] == ' ') --end;
    const size_t length = min(end - start, outputSize - 1);
    memcpy(output, text + start, length);
    output[length] = '\0';
    return true;
}

struct TopicRenderState {
    uint16_t scrollLine;
    uint16_t logicalLine = 0;
    uint8_t visibleLines;
    uint8_t drawnLines = 0;
};

void emitLine(TopicRenderState &state, const char *text, uint16_t color) {
    if (state.logicalLine >= state.scrollLine && state.drawnLines < state.visibleLines) {
        const int16_t y = TOPIC_TOP + state.drawnLines * TOPIC_LINE_HEIGHT;
        tft.setTextColor(color, bruceConfig.bgColor);
        tft.drawString(text == nullptr ? "" : text, 8, y, 1);
        ++state.drawnLines;
    }
    ++state.logicalLine;
}

void emitWrapped(TopicRenderState &state, const char *text, uint16_t color, size_t maxChars) {
    if (text == nullptr || text[0] == '\0') return;
    size_t offset = 0;
    char line[96];
    while (nextWrappedLine(text, offset, line, sizeof(line), maxChars)) emitLine(state, line, color);
}

void emitSection(
    TopicRenderState &state, const char *heading, const char *text, uint16_t bodyColor, size_t maxChars
) {
    if (text == nullptr || text[0] == '\0') return;
    emitLine(state, heading, MaliUI::ACCENT);
    emitWrapped(state, text, bodyColor, maxChars);
    emitLine(state, "", bodyColor);
}

uint16_t drawTopic(const WikiEntry &entry, uint16_t scrollLine) {
    MaliUI::drawHeader("Ajuda MaliOS");
    tft.fillRect(5, TOPIC_TOP - 2, tftWidth - 10, tftHeight - TOPIC_TOP - FOOTER_HEIGHT, bruceConfig.bgColor);
    tft.setTextSize(FP);

    const uint8_t visibleLines = max(1, (tftHeight - TOPIC_TOP - FOOTER_HEIGHT) / TOPIC_LINE_HEIGHT);
    const size_t maxChars = max(12, (tftWidth - 22) / LW);
    TopicRenderState state{scrollLine, 0, visibleLines, 0};

    emitWrapped(state, entry.name, bruceConfig.priColor, maxChars);
    emitLine(state, "", bruceConfig.priColor);
    emitSection(state, "O QUE E", entry.what, MaliUI::TEXT_PRIMARY, maxChars);
    emitSection(state, "PARA QUE SERVE", entry.purpose, MaliUI::TEXT_PRIMARY, maxChars);
    emitSection(state, "EXEMPLO", entry.example, MaliUI::TEXT_PRIMARY, maxChars);
    emitSection(state, "HARDWARE", entry.hardware, MaliUI::TEXT_PRIMARY, maxChars);
    emitSection(state, "AVISO", entry.warning, MaliUI::WARNING, maxChars);

    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    if (scrollLine > 0) tft.drawString("^", tftWidth - 14, TOPIC_TOP, 1);
    if (scrollLine + visibleLines < state.logicalLine) {
        tft.drawString("v", tftWidth - 14, tftHeight - FOOTER_HEIGHT - TOPIC_LINE_HEIGHT, 1);
    }
    MaliUI::drawFooter("Girar:rolar Seg:voltar");
    return state.logicalLine;
}

void openTopic(const WikiEntry &entry) {
    tft.fillScreen(MaliUI::BACKGROUND);
    uint16_t scrollLine = 0;
    uint16_t totalLines = drawTopic(entry, scrollLine);
    const uint8_t visibleLines = max(1, (tftHeight - TOPIC_TOP - FOOTER_HEIGHT) / TOPIC_LINE_HEIGHT);

    MaliUI::Input input;
    while (!returnToMenu) {
        auto e = input.read();
        if (e.back || e.select) return;
        if (e.steps) {
            int64_t next=int64_t(scrollLine)+e.steps;
            int maximum=max(0,int(totalLines)-visibleLines);
            uint16_t bounded=next<0?0:next>maximum?maximum:next;
            if(bounded!=scrollLine){scrollLine=bounded;totalLines=drawTopic(entry,scrollLine);}
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

uint8_t visibleListRows() { return max(1, (tftHeight - LIST_TOP - FOOTER_HEIGHT) / LIST_ROW_HEIGHT); }

void drawCategory(Category category, size_t selected, size_t count) {
    MaliUI::drawHeader("Ajuda MaliOS");
    tft.fillRect(5, 25, tftWidth - 10, tftHeight - 25 - FOOTER_HEIGHT, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(MaliWiki::categoryName(category), 8, 25, 1);

    const uint8_t rows = visibleListRows();
    size_t first = 0;
    if (selected >= rows) first = selected - rows + 1;
    if (count > rows && first + rows > count) first = count - rows;
    const size_t maxChars = max(10, (tftWidth - 30) / LW);

    for (uint8_t row = 0; row < rows && first + row < count; ++row) {
        const size_t ordinal = first + row;
        const WikiEntry *entry = entryAt(category, ordinal);
        if (entry == nullptr) break;
        const int16_t y = LIST_TOP + row * LIST_ROW_HEIGHT;
        const bool active = ordinal == selected;
        const uint16_t background = active ? MaliUI::SURFACE_ALT : bruceConfig.bgColor;
        const uint16_t foreground = active ? MaliUI::TEXT_PRIMARY : MaliUI::TEXT_SECONDARY;
        MaliUI::drawRoundedFill(6,y,tftWidth-12,LIST_ROW_HEIGHT-1,background);
        if(active)MaliUI::drawRoundedBox(6,y,tftWidth-12,LIST_ROW_HEIGHT-1,MaliUI::ACCENT);
        tft.setTextColor(foreground, background);

        char label[72];
        copyClipped(entry->name, label, sizeof(label), maxChars);
        tft.drawString(active ? ">" : " ", 9, y + 3, 1);
        tft.drawString(label, 20, y + 3, 1);
    }

    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    if (first > 0) tft.drawString("^", tftWidth - 14, LIST_TOP, 1);
    if (first + rows < count) tft.drawString("v", tftWidth - 14, tftHeight - FOOTER_HEIGHT - 10, 1);
    MaliUI::drawFooter("Girar OK:abrir Seg:voltar");
}
} // namespace

namespace MaliWiki {
const char *categoryName(Category category) {
    switch (category) {
        case Category::WIFI: return "REDE / WI-FI";
        case Category::BLE: return "BLE";
        case Category::SUB_GHZ: return "RF / SUB-GHZ";
        case Category::NRF24: return "NRF24";
        case Category::LORA: return "LORA";
        case Category::FM_RADIO: return "FM / SI4713";
        case Category::IR: return "INFRAVERMELHO";
        case Category::ETHERNET: return "ETHERNET";
        case Category::GPS: return "GPS";
        case Category::RFID: return "NFC / RFID";
        case Category::FILES: return "ARQUIVOS";
        case Category::SCRIPTS: return "SCRIPTS JS";
        case Category::CLOCK: return "RELOGIO";
        case Category::OTHERS: return "FERRAMENTAS";
        case Category::USB_HID: return "USB / HID";
        case Category::MALI_TOOLS: return "FERRAMENTAS MALI";
        case Category::MALI_COUNTER: return "MALI COUNTER";
        case Category::MALI_KEYS: return "CHAVES";
    }
    return "AJUDA MALIOS";
}

size_t documentedToolCount() { return WIKI_ENTRY_COUNT; }

void open(Category category) {
    const size_t count = countEntries(category);
    if (count == 0) {
        displayError("Sem ajuda para esta categoria", true);
        return;
    }

    size_t selected = 0;
    tft.fillScreen(MaliUI::BACKGROUND);
    drawCategory(category, selected, count);
    MaliUI::Input input;
    while (!returnToMenu) {
        auto e=input.read();
        if(e.back)return;
        if(e.steps) {
            selected=MaliUI::wrap(int64_t(selected)+e.steps,int(count));
            drawCategory(category,selected,count);
        } else if(e.select) {
            const WikiEntry *entry = entryAt(category, selected);
            if (entry != nullptr) openTopic(*entry);
            input=MaliUI::Input();
            drawCategory(category, selected, count);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
} // namespace MaliWiki
