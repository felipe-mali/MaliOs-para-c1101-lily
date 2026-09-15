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
    WIKI_ENTRY(
        WIFI, "Wi-Fi Inspector",
        "Descobre dispositivos acessiveis da rede conectada, consulta portas selecionadas e guarda referencias por MAC.",
        "Comparar scans, nomear equipamentos proprios e destacar MACs ainda nao vistos nesta rede/AP.",
        "Situacao: inventariar sua rede.\n1. Abra Wi-Fi > Wi-Fi Inspector > Escanear Rede.\n2. Confira a lista e os detalhes.\n3. Em Conhecidos, crie a baseline apenas depois de reconhecer os aparelhos.\nResultado: novas observacoes aparecem com [!]. Consulte a ajuda interna para limites e gerenciamento.",
        "Wi-Fi integrado, conexao como cliente e SD ou LittleFS para historico.",
        "Somente rede propria ou autorizada. A versao generica nao oferece desconexao administrativa nem envia ataques como alternativa."
    ),
    WIKI_ENTRY(
        WIFI_INSPECTOR, "Escanear Rede",
        "Mostra IP local, mascara, gateway, SSID, BSSID e sub-rede antes da descoberta incremental.",
        "Inventariar hosts acessiveis sem varrer todas as portas ou alterar a conexao Wi-Fi.",
        "Situacao: T-Embed em 192.168.15.20/24.\n1. Confira gateway e SSID reais.\n2. Execute Escanear Rede.\n3. Acompanhe fase, percentual e IP atual; clique ou segure para cancelar.\nResultado: ate 64 dispositivos por scan, 254 IPs por faixa e limite de 90 s. Cancelamento preserva observacoes positivas sem concluir ausencias.",
        "ESP32-S3 conectado a Wi-Fi; um socket TCP por vez.",
        "ARP pode conter cache recente. Isolamento entre clientes, filtros ou equipamentos em repouso podem limitar a descoberta."
    ),
    WIKI_ENTRY(
        WIFI_INSPECTOR, "Dispositivos e detalhes",
        "Lista IP, MAC disponivel, fabricante local, nome, portas TCP e anuncios recebidos por DNS/mDNS ou SSDP.",
        "Ajudar a reconhecer impressoras, computadores e outros dispositivos por indicios.",
        "Situacao: descobrir uma impressora propria.\n1. Abra Dispositivos Encontrados e selecione o IP.\n2. Confira se respondeu em 631 IPP ou 9100 RAW Printing.\n3. Compare MAC e nome com a etiqueta do equipamento.\nResultado: Provavel impressora e uma hipotese; porta aberta e nome anunciado nao confirmam modelo ou proprietario.",
        "Tabela OUI local pequena, sem consulta de fabricante pela Internet.",
        "Fabricante Desconhecido pode apenas indicar prefixo ausente da base. mDNS usa UDP 5353, separado dos testes TCP."
    ),
    WIKI_ENTRY(
        WIFI_INSPECTOR, "Conhecidos e baseline",
        "Associa conhecido, nome e observacao ao MAC dentro da rede/AP atual.",
        "Preservar uma referencia mesmo quando DHCP altera o endereco IP.",
        "Situacao: registrar a impressora do escritorio.\n1. Nos detalhes, abra Gerenciamento e marque como conhecido.\n2. Renomeie para Impressora Financeiro e adicione uma nota.\n3. Para um inventario conferido, use Conhecidos > Criar Baseline Atual e confirme a quantidade.\nResultado: [V] identifica conhecidos. A baseline nao verifica automaticamente quem e confiavel.",
        "SD montado ou LittleFS, escolhido ao abrir o Inspector.",
        "Observacoes sem MAC nao viram identidade persistente. Baseline inclui somente MACs encontrados e presentes no historico limitado."
    ),
    WIKI_ENTRY(
        WIFI_INSPECTOR, "Novos e MAC privado",
        "Marca com [!] os MACs observados pela primeira vez nesta base; [?] indica nao identificado e [V] conhecido.",
        "Destacar mudancas sem confundir endereco aleatorio com identidade permanente.",
        "Situacao: seu celular aparece com outro MAC.\n1. Abra Novos Dispositivos.\n2. Confira o aviso de MAC privado/aleatorio.\n3. Compare com o endereco privado mostrado nas configuracoes do proprio celular.\nResultado: novo MAC pode pertencer ao mesmo aparelho; nunca identifica automaticamente uma pessoa.",
        "Deteccao do bit de endereco localmente administrado.",
        "Um MAC local pode ser fixo ou aleatorio. Nao se infere fabricante por seu prefixo, mesmo que pareca conhecido."
    ),
    WIKI_ENTRY(
        WIFI_INSPECTOR, "Atualizar e alteracoes",
        "Compara as observacoes da sessao com a varredura anterior e mostra novos, encontrados e nao encontrados.",
        "Acompanhar mudancas durante um inventario manual.",
        "Situacao: primeiro scan encontrou 12 hosts e o seguinte 13.\n1. Use Atualizar scan.\n2. Consulte Resultado / alteracoes e Novos Dispositivos.\n3. Confira o novo MAC nos detalhes.\nResultado: diferenca de totais nao e necessariamente um unico aparelho novo; outro pode ter deixado de responder no mesmo intervalo.",
        "Execucao manual e progressiva; sem tarefa permanente de scan.",
        "Nao encontrado atualmente nao significa que saiu definitivamente. Em scan parcial nao se concluem ausencias."
    ),
    WIKI_ENTRY(
        WIFI_INSPECTOR, "Historico e armazenamento",
        "Guarda ate 96 MACs por rede/AP, com nomes, notas, portas, datas disponiveis e contagem de scans em que foram vistos.",
        "Consultar referencias sem reescrever a flash a cada pacote recebido.",
        "Situacao: procurar um aparelho que nao respondeu hoje.\n1. Abra Historico e selecione o MAC.\n2. Confira ultimo IP, nome e ultimo avistamento.\n3. Se houve erro de gravacao, tente Configuracoes > Salvar historico antes de sair.\nResultado: datas dependem do relogio sincronizado; sem hora valida a tela informa essa limitacao.",
        "Pasta /MaliInspector/, formato MWI1 com CRC32, temporario e backup; ate oito bases de rede/AP.",
        "As bases distinguem BSSID e sub-rede: trocar de AP pode abrir outra base. Arquivo corrompido fica protegido ate limpeza explicita."
    ),
    WIKI_ENTRY(
        WIFI_INSPECTOR, "Gerenciamento do roteador",
        "Oferece interface comum para adaptadores administrativos. Esta versao inclui apenas Generico, sem API de desconexao habilitada.",
        "Preparar integracao legitima sem inventar endpoints ou substituir falta de suporte por ataques de radio.",
        "Situacao: solicitar desconexao de um cliente proprio.\n1. Confira MAC e IP nos detalhes atuais.\n2. Abra Gerenciamento > Desconectar da rede e confirme.\nResultado: no adaptador Generico, a tela informa falta de suporte e nenhum comando e enviado. Nao ha bloqueio permanente, jamming nem deauth spoofada.",
        "Adaptador compativel e autenticacao serao necessarios para controle administrativo real.",
        "Gerenciamento do Roteador informa o suporte. O adaptador Generico nao solicita nem armazena usuario ou senha; nao e uma implementacao OpenWrt/MikroTik/UniFi."
    ),
    WIKI_ENTRY(
        WIFI_INSPECTOR, "Configuracoes e limites",
        "Permite ativar portas TCP, nomes e SSDP, escolher timeout e inicio da faixa, consultar rede e gerenciar historico.",
        "Controlar o tempo e o alcance do inventario em um dispositivo com memoria limitada.",
        "Situacao: rede /16, maior que uma faixa de 254 IPs.\n1. Confira a sub-rede real nos dados da rede.\n2. Escolha Inicio da faixa dentro dela e execute o scan.\n3. Repita em outra faixa se necessario.\nResultado: apenas a faixa indicada foi percorrida; a tela avisa sobre cobertura limitada. Ajustes de scan valem nesta sessao.",
        "Timeout TCP de 80, 120 ou 200 ms; buffers limitados e sockets fechados ao sair.",
        "Mudar a conexao cancela o scan. Volte e reabra o Inspector na rede desejada. Limpar historico exige confirmacao e remove nomes e conhecidos desta base."
    ),
    // Rede / Wi-Fi
    WIKI_ENTRY(
        WIFI,
        "Conectar ao Wi-Fi",
        "Abre a selecao de redes e conecta o MaliOS como estacao Wi-Fi.",
        "Permite usar ferramentas que dependem da rede local ou da Internet.",
        "Situacao: Abrir a WebUI em casa.\n"
        "1. Escolha sua rede de 2,4 GHz.\n"
        "2. Informe a senha e aguarde a conexao.\n"
        "3. Consulte Info do AP.\n"
        "Resultado: Anote o IP recebido. Se nao conectar, confira senha, alcance e se a rede oferece 2,4 GHz.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        WIFI,
        "Iniciar AP Wi-Fi",
        "Cria o ponto de acesso configurado no MaliOS para ate quatro clientes.",
        "Fornece uma rede local quando nao ha roteador disponivel.",
        "Situacao: Usar o aparelho sem roteador.\n"
        "1. Inicie o AP.\n"
        "2. No celular, conecte ao nome e senha configurados.\n"
        "3. Abra a WebUI pelo endereco mostrado.\n"
        "Resultado: O celular acessa uma rede local do MaliOS. Aviso de rede sem Internet nao significa falha da WebUI.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        "O AP fica visivel nas proximidades. Use senha forte e desligue-o ao terminar."
    ),
    WIKI_ENTRY(
        WIFI,
        "Desligar Wi-Fi",
        "Encerra os modos estacao e ponto de acesso e desliga o radio Wi-Fi.",
        "Libera recursos e encerra conexoes de rede ativas.",
        "Situacao: Encerrar uma sessao pelo navegador.\n"
        "1. Termine e confirme o salvamento dos arquivos.\n"
        "2. Na tela, escolha Desligar Wi-Fi.\n"
        "Resultado: O navegador perde a conexao. Para voltar, conecte o Wi-Fi ou inicie o AP e abra a WebUI novamente.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        WIFI,
        "Info do AP",
        "Mostra SSID, senha salva, RSSI, IP, gateway, canal, BSSID e seguranca da conexao.",
        "Ajuda a conferir a rede e os parametros recebidos pelo MaliOS.",
        "Situacao: Descobrir o endereco do MaliOS.\n"
        "1. Conecte ao roteador.\n"
        "2. Abra Info do AP e leia IP e SSID.\n"
        "3. Se o IP exibido for 192.168.1.50, use esse endereco no navegador.\n"
        "Resultado: O IP identifica o aparelho nessa conexao e pode mudar. Gateway e o roteador, nao o endereco da WebUI.",
        "Wi-Fi do ESP32-S3 e tela.",
        "A senha salva pode aparecer na tela. Evite exibi-la diante de terceiros."
    ),
    WIKI_ENTRY(
        WIFI,
        "WebUI",
        "Inicia a interface web do MaliOS na rede atual ou em modo AP.",
        "Gerencia arquivos do SD e LittleFS e recursos mantidos pela interface original.",
        "Situacao: Gerenciar um arquivo pelo celular.\n"
        "1. Inicie a WebUI e conecte o celular a mesma rede.\n"
        "2. Abra o endereco mostrado e autentique-se.\n"
        "3. Em Arquivos, selecione SD ou LittleFS.\n"
        "Resultado: Voce ve o armazenamento escolhido. Se bruce.local nao resolver, use o IP exibido; confirme que ambos estao na mesma rede.",
        "Wi-Fi ESP32-S3, LittleFS e microSD quando presente.",
        "Preserve a autenticacao configurada e nao exponha a interface a redes nao confiaveis."
    ),
    WIKI_ENTRY(
        WIFI,
        "Ataques Wi-Fi",
        "Abre a suite existente de testes de alvo, Karma, beacon e deautenticacao.",
        "Avalia o comportamento defensivo de uma rede sem fio controlada.",
        "Situacao: Conferir o monitoramento do AP de bancada.\n"
        "1. Registre a conexao normal de um cliente proprio.\n"
        "2. Escolha apenas o ensaio previsto no plano do laboratorio.\n"
        "3. Encerre e confira os logs do AP.\n"
        "Resultado: Compare antes, durante e depois. A ausencia de um alerta nao comprova que a rede e segura.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Ataques a alvo",
        "Varre APs e oferece informacoes, deauth, captura de handshake e clonagem de portal para o alvo escolhido.",
        "Agrupa testes ativos dirigidos a um unico AP de laboratorio.",
        "Situacao: Evitar selecionar o AP errado no ensaio.\n"
        "1. Consulte SSID, BSSID e canal do seu AP de bancada.\n"
        "2. Confira esses dados na lista de alvos antes de escolher a acao aprovada.\n"
        "Resultado: Nomes de rede iguais podem pertencer a APs diferentes. Identifique o equipamento pelos dados do seu inventario.",
        "Wi-Fi ESP32-S3; SD ou LittleFS para capturas e portal.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Ataque Karma",
        "Observa probe requests e pode anunciar SSIDs solicitados, iniciar portal e enviar deauth conforme a configuracao.",
        "Testa se clientes proprios tentam se associar a redes lembradas sem validacao adequada.",
        "Situacao: Avaliar perfis salvos em um celular de teste.\n"
        "1. Use um perfil ficticio criado para a bancada isolada.\n"
        "2. Compare o comportamento antes e depois de remover esse perfil do celular.\n"
        "Resultado: Registre se houve tentativa de associacao. Isso descreve aquele cliente e configuracao, nao todos os celulares.",
        "Wi-Fi ESP32-S3; SD ou LittleFS quando usa portal.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Spam de beacon",
        "Transmite anuncios de AP com listas, nomes aleatorios, um nome ou nomes personalizados.",
        "Testa visualizacao e filtragem de muitos SSIDs em equipamento proprio.",
        "Situacao: Avaliar a lista de redes de um scanner proprio.\n"
        "1. Registre a lista normal na bancada blindada.\n"
        "2. Compare a lista durante o ensaio aprovado e apos encerra-lo.\n"
        "Resultado: Observe atualizacao, travamento ou entradas antigas. SSIDs anunciados nao comprovam acesso a Internet.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Inundacao de deauth",
        "Percorre APs detectados e transmite quadros de deautenticacao repetidamente.",
        "Testa protecoes e monitoramento contra desconexoes forjadas.",
        "Situacao: Revisar protecoes do AP em bancada blindada.\n"
        "1. Registre conectividade e logs antes do ensaio aprovado.\n"
        "2. Ao encerrar, confira a recuperacao do cliente proprio.\n"
        "Resultado: Desconexoes e alertas devem ser confrontados com os logs. Nao conclua sobre PMF apenas pela tela do MaliOS.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Deauth avancado",
        "Oferece deauth individual, contra todos os APs vistos ou contra uma lista definida.",
        "Permite ensaios controlados de resiliencia e deteccao.",
        "Situacao: Documentar a resiliencia de um AP proprio.\n"
        "1. Confirme a identidade do AP de laboratorio.\n"
        "2. Registre o estado do cliente e os alertas durante o ensaio aprovado.\n"
        "3. Confira a reconexao ao encerrar.\n"
        "Resultado: Anote se o servico recuperou e em quanto tempo, usando uma observacao externa.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Mali Portal",
        "Inicia o portal cativo de laboratorio com o modelo seguro selecionado no Portal Studio da WebUI.",
        "Permite demonstrar navegacao cativa com dados ficticios, sem depender de servicos externos.",
        "Situacao: Demonstrar uma pagina de boas-vindas.\n"
        "1. Na WebUI, escolha o modelo Mali Lab no Portal Studio.\n"
        "2. Inicie Mali Portal.\n"
        "3. Conecte seu celular de teste e abra a pagina cativa.\n"
        "Resultado: A pagina local deve aparecer. Use informacoes ficticias; nao e necessario fornecer uma senha real de outro servico.",
        "Wi-Fi ESP32-S3 e LittleFS para os modelos.",
        "Use somente em aparelhos e redes proprios ou em laboratorio autorizado. O Mali Lab nao solicita senhas reais."
    ),
    WIKI_ENTRY(
        WIFI,
        "Evil Portal",
        "Cria AP e portal cativo DNS/HTTP com pagina padrao ou HTML do armazenamento e registra envios em CSV.",
        "Demonstra riscos de portais falsos em treinamento autorizado.",
        "Situacao: Treinar reconhecimento de portais falsos.\n"
        "1. Use uma pagina de treinamento e participantes autorizados.\n"
        "2. Envie somente dados ficticios.\n"
        "3. Confira o registro CSV do exercicio.\n"
        "Resultado: O envio fica registrado. O CSV demonstra a coleta do formulario, nao autentica o participante em um servico real.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        WARN_CREDENTIALS
    ),
    WIKI_ENTRY(
        WIFI,
        "NetCut",
        "Descobre hosts por ARP e oferece corte, restauracao e teste por envenenamento ARP.",
        "Avalia segmentacao e deteccao de manipulacao ARP em uma LAN controlada.",
        "Situacao: Verificar recuperacao de uma LAN isolada.\n"
        "1. Use dois hosts de bancada e registre a conectividade inicial.\n"
        "2. Apos o ensaio aprovado, aplique a restauracao e confira os dois hosts.\n"
        "Resultado: Confirme externamente que o trafego normal voltou. A tentativa de restauracao nao garante que todas as tabelas foram corrigidas.",
        "Wi-Fi ESP32-S3 e LittleFS para a lista VIP.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Escutar TCP",
        "Abre um servidor TCP na porta escolhida e troca texto com um cliente.",
        "Testa comunicacao TCP simples na rede local.",
        "Situacao: Receber texto de um computador proprio.\n"
        "1. Escolha uma porta livre, por exemplo 5000.\n"
        "2. No cliente TCP do PC, conecte ao IP do MaliOS e a mesma porta.\n"
        "3. Envie ola.\n"
        "Resultado: A mensagem deve aparecer na sessao. Conexao recusada pede conferir IP, porta, servidor iniciado e rede local.",
        "Wi-Fi integrado do ESP32-S3.",
        "Abra portas somente em rede confiavel e encerre o servidor ao terminar."
    ),
    WIKI_ENTRY(
        WIFI,
        "Cliente TCP",
        "Abre uma conexao TCP bruta para o IP e a porta informados e troca texto.",
        "Ajuda a testar um servico TCP simples sob seu controle.",
        "Situacao: Conferir um servidor de eco seu.\n"
        "1. Inicie o servidor de eco no PC.\n"
        "2. Informe seu IP e porta no MaliOS.\n"
        "3. Envie teste123.\n"
        "Resultado: Se o servidor for de eco, ele devolve teste123. Um servico que nao responde texto simples pode conectar sem mostrar uma resposta legivel.",
        "Wi-Fi integrado do ESP32-S3.",
        "Conecte somente a servicos proprios ou autorizados."
    ),
    WIKI_ENTRY(
        WIFI,
        "Proxy SOCKS4",
        "Inicia um proxy SOCKS4 e SOCKS4a TCP na porta 1080, sem autenticacao implementada.",
        "Encaminha trafego TCP de um cliente da rede por meio do MaliOS.",
        "Situacao: Verificar um cliente em LAN isolada.\n"
        "1. Inicie o proxy.\n"
        "2. Configure seu cliente SOCKS4 com o IP do MaliOS e porta 1080.\n"
        "3. Acesse um servico de teste proprio.\n"
        "Resultado: O cliente deve usar o proxy. Encerre a sessao ao concluir; nao existe pedido de login nesse proxy.",
        "Wi-Fi integrado do ESP32-S3.",
        "Nao exponha o proxy a clientes nao confiaveis; ele nao exige login."
    ),
    WIKI_ENTRY(
        WIFI,
        "TelNET",
        "Cliente Telnet interativo para host e porta escolhidos, com log opcional.",
        "Administra ou testa um servico Telnet legado autorizado.",
        "Situacao: Consultar um equipamento legado de bancada.\n"
        "1. Informe o IP, a porta do servico e sua conta de teste.\n"
        "2. Execute apenas uma consulta de estado prevista no manual do equipamento.\n"
        "3. Encerre a sessao.\n"
        "Resultado: O retorno vem do equipamento remoto. Falta de conexao pode indicar porta errada ou servico desativado.",
        "Wi-Fi integrado do ESP32-S3; armazenamento quando o log esta ativo.",
        "Telnet nao cifra senha nem comandos. Use apenas servidor autorizado e rede protegida."
    ),
    WIKI_ENTRY(
        WIFI,
        "SSH",
        "Cliente SSH interativo que solicita host, porta, usuario e senha e roda em tarefa propria.",
        "Permite administrar um servidor autorizado pelo T-Embed.",
        "Situacao: Consultar o nome de um servidor Linux proprio.\n"
        "1. Informe o IP do servidor, sua porta SSH e sua conta.\n"
        "2. Apos conectar, digite hostname.\n"
        "3. Use exit para encerrar.\n"
        "Resultado: Aparece o nome configurado no servidor. Se falhar, confira credenciais, porta e acesso pela mesma rede.",
        "Wi-Fi integrado do ESP32-S3; armazenamento se o log estiver ativo.",
        "Use apenas contas e servidores autorizados e proteja os registros de sessao."
    ),
    WIKI_ENTRY(
        WIFI,
        "Sniffer",
        "Captura quadros Wi-Fi em modo promiscuo: PCAP completo, EAPOL ou deauth; um modo pode transmitir deauth.",
        "Registra trafego para diagnostico e estudo de uma rede controlada.",
        "Situacao: Analisar trafego do seu AP de laboratorio.\n"
        "1. Escolha captura passiva PCAP e armazenamento disponivel.\n"
        "2. Gere trafego normal entre seus aparelhos.\n"
        "3. Encerre e abra o arquivo em um analisador PCAP.\n"
        "Resultado: O arquivo contem quadros observados pelo receptor. Nao representa todo o trafego e pode nao conter o conteudo cifrado legivel.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Analisar canal",
        "Estima a ocupacao dos canais 1 a 11 usando quadros recebidos, RSSI e tempo de amostragem.",
        "Ajuda a comparar atividade entre canais Wi-Fi; nao e analisador de espectro RF.",
        "Situacao: Comparar canais perto do seu roteador.\n"
        "1. Observe a mesma posicao por alguns ciclos.\n"
        "2. Anote quais canais aparecem mais ativos.\n"
        "3. Repita em outro horario.\n"
        "Resultado: Compare tendencias. Um canal menos ativo nesta amostra pode estar ocupado depois; nao e uma medicao de toda a energia RF.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        "Analise passiva; identificadores observados ainda devem ser tratados com privacidade."
    ),
    WIKI_ENTRY(
        WIFI,
        "Detectar jammer",
        "Conta quadros deauth e disassoc nos canais 1 a 11 e compara com um limite.",
        "Indica possivel abuso desses quadros; nao detecta toda interferencia de radio.",
        "Situacao: Investigar quedas no Wi-Fi proprio.\n"
        "1. Observe os contadores durante uso normal.\n"
        "2. Compare o horario de um alerta com os logs do seu roteador.\n"
        "Resultado: O alerta se refere a deauth/disassoc observados. Queda sem alerta pode ter outra causa; alerta isolado nao prova interferencia intencional.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        "Funcao passiva; uma indicacao nao prova, sozinha, a existencia de jammer."
    ),
    WIKI_ENTRY(
        WIFI,
        "Procurar hosts",
        "Envia consultas ARP pela sub-rede e lista IP e MAC dos hosts encontrados.",
        "Inventaria dispositivos de uma LAN autorizada e abre acoes por host.",
        "Situacao: Localizar uma placa de desenvolvimento na LAN.\n"
        "1. Conecte MaliOS e placa ao mesmo roteador.\n"
        "2. Execute a busca.\n"
        "3. Compare o MAC encontrado com o da placa.\n"
        "Resultado: O IP correspondente pode ser usado nas ferramentas locais. Um host ausente pode estar isolado, desligado ou nao ter respondido.",
        "Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Info do host",
        "Mostra fabricante do MAC e tenta uma lista fixa de portas TCP com timeout curto.",
        "Ajuda a reconhecer um host encontrado pela busca ARP.",
        "Situacao: Conferir servicos do seu servidor de testes.\n"
        "1. Selecione o IP correto em Procurar hosts.\n"
        "2. Abra Info do host.\n"
        "3. Compare as portas mostradas com os servicos que voce ativou.\n"
        "Resultado: Uma porta sem resposta pode estar filtrada. A lista fixa e o timeout curto nao equivalem a inventario completo.",
        "Wi-Fi ESP32-S3; W5500 quando aberto pela busca Ethernet.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "SSH do host",
        "Abre o cliente SSH para o host selecionado na varredura.",
        "Facilita o acesso autorizado a um servidor encontrado.",
        "Situacao: Entrar na placa encontrada na rede.\n"
        "1. Confira o IP na lista de hosts.\n"
        "2. Escolha SSH e use sua conta de teste.\n"
        "3. Consulte hostname e saia com exit.\n"
        "Resultado: Confirme que o nome devolvido corresponde a placa esperada antes de administrar o sistema.",
        "Wi-Fi integrado do ESP32-S3.",
        "Use somente servidor e credenciais autorizados."
    ),
    WIKI_ENTRY(
        WIFI,
        "Desautenticar estacao",
        "Transmite quadros Wi-Fi de deauth ou disassoc para a estacao escolhida.",
        "Testa protecao e alerta de uma rede controlada.",
        "Situacao: Conferir um cliente em bancada Wi-Fi isolada.\n"
        "1. Identifique seu cliente e AP no inventario.\n"
        "2. Compare conectividade e logs no ensaio aprovado.\n"
        "3. Confira a recuperacao ao encerrar.\n"
        "Resultado: Registre o comportamento desse par AP/cliente. Um resultado isolado nao certifica protecao PMF.",
        "Radio Wi-Fi integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Falsificacao ARP",
        "Envia respostas ARP falsas entre alvo e gateway e tenta restaurar a tabela ao parar.",
        "Avalia deteccao e isolamento contra ARP spoofing.",
        "Situacao: Avaliar alertas ARP na sua LAN isolada.\n"
        "1. Registre as tabelas ARP legitimas dos hosts de teste.\n"
        "2. Compare os alertas do monitor durante o ensaio aprovado.\n"
        "3. Confira as tabelas ao parar.\n"
        "Resultado: Valide que o gateway voltou ao MAC correto. A tentativa de restauracao precisa ser conferida nos hosts.",
        "Wi-Fi ou W5500 conforme a origem; armazenamento para PCAP.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Envenenamento ARP",
        "Anuncia mapeamentos ARP e MAC aleatorios para os hosts encontrados e pode registrar PCAP.",
        "Submete uma LAN de laboratorio a uma condicao agressiva de tabela ARP.",
        "Situacao: Validar isolamento de hosts descartaveis.\n"
        "1. Guarde a configuracao e os mapeamentos normais da bancada.\n"
        "2. Compare os logs de defesa durante o ensaio aprovado.\n"
        "3. Ao fim, valide o acesso ao gateway.\n"
        "Resultado: Anote quais protecoes alertaram ou bloquearam. Ausencia de conectividade nao identifica sozinha qual protecao atuou.",
        "W5500 via SPI e microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Esgotamento DHCP",
        "Envia DHCP Discover com enderecos MAC aleatorios ate o usuario parar.",
        "Testa protecoes do servidor DHCP contra consumo do pool.",
        "Situacao: Avaliar o monitor de um servidor DHCP descartavel.\n"
        "1. Registre o numero de concessoes livres na bancada isolada.\n"
        "2. Compare alertas no ensaio aprovado.\n"
        "3. Ao encerrar, confira o pool no servidor.\n"
        "Resultado: Uma nova placa de teste deve voltar a obter endereco. Confira a recuperacao diretamente no servidor DHCP.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Inundacao MAC",
        "Envia quadros com MAC e IP aleatorios para pressionar a tabela CAM do switch.",
        "Testa limites e alertas de um switch controlado.",
        "Situacao: Avaliar alertas de um switch de bancada.\n"
        "1. Registre o estado normal das portas e tabela MAC.\n"
        "2. Compare esses dados durante o ensaio aprovado.\n"
        "3. Confira o funcionamento apos encerrar.\n"
        "Resultado: Use os logs do switch para distinguir bloqueio, limite de tabela e falha de enlace.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Wireguard",
        "Le o arquivo /wg.conf do microSD, sincroniza a hora e inicia um tunel WireGuard.",
        "Conecta o MaliOS a uma rede privada configurada pelo usuario.",
        "Situacao: Conectar ao laboratorio por VPN propria.\n"
        "1. Coloque sua configuracao em /wg.conf no SD.\n"
        "2. Conecte o Wi-Fi e confira a hora.\n"
        "3. Inicie Wireguard e teste um servico permitido pelo tunel.\n"
        "Resultado: O acesso depende de chaves, rotas e servidor corretos. Nao compartilhe wg.conf: ele pode conter chave privada.",
        "Wi-Fi ESP32-S3 e microSD.",
        "A configuracao e as chaves podem aparecer na Serial. Trate o log como secreto."
    ),
    WIKI_ENTRY(
        WIFI,
        "Responder",
        "Responde a NBNS e LLMNR, oferece SMB e registra respostas NTLMv2 no armazenamento.",
        "Demonstra riscos de resolucao de nomes e autenticacao em rede Windows controlada.",
        "Situacao: Validar resolucao de nomes em Windows de teste.\n"
        "1. Use apenas uma VM descartavel e contas ficticias na LAN isolada.\n"
        "2. Compare os logs do exercicio antes e depois de aplicar o endurecimento planejado.\n"
        "Resultado: Verifique se a autenticacao inesperada deixou de ocorrer. Trate os registros como dados de autenticacao, mesmo em laboratorio.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        WARN_CREDENTIALS
    ),
    WIKI_ENTRY(
        WIFI,
        "Brucegotchi",
        "Descobre APs, interage com deauth, aguarda handshakes e anuncia beacons pwngrid em uma maquina de estados.",
        "Automatiza experimentos de monitoramento Wi-Fi em laboratorio.",
        "Situacao: Observar a automacao em uma bancada blindada.\n"
        "1. Identifique apenas o AP e cliente de teste.\n"
        "2. Acompanhe as mudancas de estado durante o ensaio aprovado.\n"
        "3. Ao terminar, confira capturas e conectividade.\n"
        "Resultado: Um estado de captura nao garante um handshake completo; valide o arquivo gerado.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        WIFI,
        "Recuperar senha",
        "Testa offline palavras de uma wordlist contra um handshake salvo em PCAP.",
        "Verifica a resistencia de uma senha de rede cuja captura e autorizada.",
        "Situacao: Conferir uma senha ficticia de um AP proprio.\n"
        "1. Use um PCAP autorizado e uma lista pequena preparada para o exercicio.\n"
        "2. Compare a conclusao com a senha conhecida do AP de teste.\n"
        "Resultado: Nao encontrar a senha pode significar que ela nao esta na lista ou que a captura e inadequada; nao prova resistencia absoluta.",
        "CPU ESP32-S3, microSD ou LittleFS; nao transmite radio durante o teste.",
        WARN_CREDENTIALS
    ),

    // Bluetooth Low Energy
    WIKI_ENTRY(
        BLE,
        "Comandos de midia",
        "Emula um controle HID BLE com play, pausa, faixas, volume, mute e captura de tela.",
        "Controla midia de um host pareado conscientemente.",
        "Situacao: Pausar uma musica no computador proprio.\n"
        "1. Pareie o controle BLE com o computador.\n"
        "2. Abra o reprodutor e inicie uma musica.\n"
        "3. Envie Pausar e depois Reproduzir.\n"
        "Resultado: O aplicativo compativel deve reagir. A resposta depende do host e do aplicativo em foco.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Use somente dispositivo proprio e pareado."
    ),
    WIKI_ENTRY(
        BLE,
        "Procurar BLE",
        "Faz scan ativo por cinco segundos e lista nome, endereco e RSSI de ate cem dispositivos.",
        "Ajuda a localizar e identificar perifericos BLE proximos.",
        "Situacao: Localizar um sensor seu.\n"
        "1. Ligue o sensor e execute a busca.\n"
        "2. Anote nome e RSSI.\n"
        "3. Afaste o sensor e repita.\n"
        "Resultado: O RSSI costuma ficar mais negativo com sinal mais fraco; nao fornece distancia exata. Enderecos podem mudar.",
        "Bluetooth LE integrado do ESP32-S3.",
        "A busca e passiva para o usuario, mas respeite a privacidade dos identificadores vistos."
    ),
    WIKI_ENTRY(
        BLE,
        "iBeacon",
        "Anuncia continuamente um iBeacon de demonstracao com UUID fixo e MAC aleatorio.",
        "Testa aplicativos e scanners que reconhecem beacons BLE.",
        "Situacao: Testar um aplicativo leitor de beacons.\n"
        "1. Inicie iBeacon no MaliOS.\n"
        "2. Abra o leitor no seu celular.\n"
        "3. Encerre o anuncio e observe a lista apos o timeout do leitor.\n"
        "Resultado: O beacon deve aparecer enquanto e recebido. A entrada pode continuar visivel por algum tempo depois da parada.",
        "Bluetooth LE integrado do ESP32-S3.",
        "O anuncio fica visivel nas proximidades; use em ambiente autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Bad BLE",
        "Carrega DuckyScript do SD ou LittleFS, apresenta teclado BLE e executa o script apos pareamento.",
        "Automatiza entradas de teclado em um host de teste.",
        "Situacao: Digitar uma frase de teste no seu PC.\n"
        "1. Abra um editor vazio no PC pareado.\n"
        "2. Revise um DuckyScript contendo apenas STRING Ola MaliOS.\n"
        "3. Execute e confira o texto.\n"
        "Resultado: A frase vai para a janela em foco. Layout de teclado e foco incorretos podem mudar o resultado.",
        "BLE ESP32-S3, microSD ou LittleFS.",
        WARN_HID
    ),
    WIKI_ENTRY(
        BLE,
        "Teclado BLE",
        "Oferece teclado HID BLE interativo, teclas especiais e fila de comandos pelo encoder.",
        "Digita e controla um host pareado sem teclado fisico.",
        "Situacao: Escrever uma nota no tablet proprio.\n"
        "1. Pareie o MaliOS e abra uma nota vazia.\n"
        "2. Envie Ola MaliOS pelo teclado.\n"
        "3. Teste uma tecla de apagar.\n"
        "Resultado: O texto e a edicao devem aparecer na nota. Se nada ocorrer, confira pareamento e campo em foco.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Use somente host proprio e confira o campo que recebera as teclas."
    ),
    WIKI_ENTRY(
        BLE,
        "Spam BLE",
        "Transmite repetidamente anuncios de varios ecossistemas, com intervalo, potencia e MAC configuraveis.",
        "Testa filtragem e comportamento de interfaces diante de muitos anuncios BLE.",
        "Situacao: Avaliar filtragem de anuncios em telefone de teste.\n"
        "1. Registre a tela do scanner na bancada blindada.\n"
        "2. Compare durante o ensaio aprovado e apos encerrar.\n"
        "Resultado: Observe entradas repetidas e recuperacao da interface. Muitos anuncios nao significam muitos dispositivos fisicos.",
        "Bluetooth LE ESP32-S3 e NVS para configuracao.",
        "Pode gerar pop-ups e perturbar dispositivos proximos. Use apenas em laboratorio autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Suite BLE",
        "Reune perfil, conexoes, writes, fuzzing, HID, FastPair, HFP, testes de indisponibilidade e captura.",
        "Centraliza ensaios experimentais de seguranca BLE em alvos controlados.",
        "Situacao: Revisar um prototipo de sensor proprio.\n"
        "1. Comece pela descoberta e perfil detalhado.\n"
        "2. Registre os servicos encontrados.\n"
        "3. Execute apenas os casos previstos no plano, um por vez.\n"
        "Resultado: Compare cada resultado com os logs do prototipo. Rotulos experimentais nao confirmam uma vulnerabilidade.",
        "BLE ESP32-S3; nRF24 em uma rotina; armazenamento para captura.",
        "Os resultados sao experimentais e as rotinas podem travar ou alterar o alvo. Somente laboratorio autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Busca de vulnerabilidades",
        "Seleciona um alvo e combina verificacoes experimentais relacionadas a HFP e FastPair.",
        "Faz uma triagem inicial, sem garantir que um resultado seja uma vulnerabilidade real.",
        "Situacao: Fazer triagem de um acessorio em desenvolvimento.\n"
        "1. Identifique seu alvo de bancada.\n"
        "2. Guarde os logs antes e depois da verificacao.\n"
        "3. Repita somente o caso planejado para conferir consistencia.\n"
        "Resultado: Um alerta e uma pista a investigar. Ausencia de alerta nao significa que o acessorio nao possui falhas.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Perfil detalhado",
        "Conecta ao dispositivo e descobre servicos, caracteristicas e possibilidades de escrita.",
        "Mapeia a interface GATT de um periferico autorizado.",
        "Situacao: Conferir a interface GATT do seu sensor.\n"
        "1. Escolha o sensor correto.\n"
        "2. Abra o perfil detalhado.\n"
        "3. Compare UUIDs e propriedades com a documentacao do seu firmware.\n"
        "Resultado: Servico encontrado confirma descoberta nesta sessao. Permissao de escrita anunciada nao garante que qualquer valor sera aceito.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Conecte somente a dispositivos proprios ou autorizados."
    ),
    WIKI_ENTRY(
        BLE,
        "Ataques FastPair",
        "Agrupa testes experimentais de estado, handshake, popup, conexao e memoria ligados ao Fast Pair.",
        "Avalia como um dispositivo de laboratorio reage a entradas inesperadas.",
        "Situacao: Revisar o pareamento de um acessorio proprio.\n"
        "1. Registre o estado normal do prototipo isolado.\n"
        "2. Compare logs e comportamento no caso de teste aprovado.\n"
        "3. Confira o pareamento normal ao terminar.\n"
        "Resultado: Um popup ou desconexao nao comprova comprometimento. Registre o efeito observado e a recuperacao.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Ataques HFP",
        "Agrupa testes de conexao Hands-Free, cadeia HFP e transicao experimental HFP para HID.",
        "Avalia a robustez de perfis de telefonia em equipamento controlado.",
        "Situacao: Avaliar um headset de desenvolvimento.\n"
        "1. Registre o estado de conexao na bancada.\n"
        "2. Compare os logs durante o caso aprovado.\n"
        "3. Ao encerrar, confira novamente uma conexao normal.\n"
        "Resultado: Diferencie falha de conexao de falha do dispositivo. Os testes sao experimentais e dependem do suporte do alvo.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Ferramentas de audio",
        "Reune testes AVRCP, alerta de telefonia e rotinas experimentais de falha de audio.",
        "Observa a resposta de um acessorio de audio proprio a comandos inesperados.",
        "Situacao: Conferir uma caixa de som propria.\n"
        "1. Reproduza um som de teste em volume baixo.\n"
        "2. Compare resposta e logs no caso aprovado.\n"
        "3. Confira o controle normal ao terminar.\n"
        "Resultado: Anote pausas, erros e necessidade de reconexao; nao atribua a causa apenas ao nome do teste.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Ataques HID",
        "Reune conexao, envio de teclas, DuckyScript e rotinas HID experimentais por sistema.",
        "Testa controles de pareamento e entrada de um host de laboratorio.",
        "Situacao: Conferir entrada em computador descartavel.\n"
        "1. Abra um editor vazio no host autorizado.\n"
        "2. Use apenas uma sequencia de texto inofensiva do plano.\n"
        "3. Observe se foi exigido pareamento.\n"
        "Resultado: Registre se o texto chegou e qual autorizacao ocorreu. Nao execute sequencias em uma janela de terminal.",
        "Bluetooth LE ESP32-S3; armazenamento para scripts.",
        WARN_HID
    ),
    WIKI_ENTRY(
        BLE,
        "Corrupcao de memoria",
        "Executa testes FastPair rotulados como memoria, estado e handshake com entradas anormais.",
        "Procura falhas de robustez em um dispositivo BLE de desenvolvimento.",
        "Situacao: Investigar reinicio de um prototipo BLE.\n"
        "1. Guarde logs e versao do firmware de bancada.\n"
        "2. Compare o estado antes e depois do caso aprovado.\n"
        "Resultado: Um reinicio e um sintoma, nao prova de corrupcao de memoria. A confirmacao exige diagnostico do proprio prototipo.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Ataques DoS",
        "Inclui flood de conexao, spam de anuncios, Jam & Connect com nRF24 e fuzzer.",
        "Testa disponibilidade e recuperacao de um alvo de laboratorio.",
        "Situacao: Avaliar recuperacao de um prototipo isolado.\n"
        "1. Anote se ele anuncia e aceita a conexao normal.\n"
        "2. Apos o ensaio aprovado, repita essas observacoes.\n"
        "Resultado: Registre se recuperou sozinho ou precisou reiniciar. O resultado vale para o caso observado, sem garantir disponibilidade geral.",
        "BLE ESP32-S3 e nRF24 externo em uma das rotinas.",
        "Pode indisponibilizar o alvo e interferir em 2,4 GHz. Use somente laboratorio isolado autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Entrega de payload",
        "Agrupa DuckyScript e tentativas experimentais relacionadas a PIN e autenticacao.",
        "Avalia controles de entrada e autorizacao de um host BLE proprio.",
        "Situacao: Validar entrada autorizada em PC de teste.\n"
        "1. Revise uma sequencia contendo apenas uma frase.\n"
        "2. Abra um editor vazio no host.\n"
        "3. Confira o efeito no caso aprovado.\n"
        "Resultado: O texto esperado deve ser a unica alteracao. Compare qualquer divergencia com o layout e o foco da janela.",
        "Bluetooth LE ESP32-S3 e armazenamento para scripts.",
        WARN_HID
    ),
    WIKI_ENTRY(
        BLE,
        "Ferramentas de teste",
        "Reune descoberta de escrita, teste de audio, fuzzer e teste HID.",
        "Apoia diagnostico de um periferico BLE em desenvolvimento.",
        "Situacao: Comparar duas versoes do seu sensor BLE.\n"
        "1. Registre o perfil e os logs da versao atual.\n"
        "2. Repita o mesmo caso de bancada com a outra versao.\n"
        "Resultado: Compare os mesmos campos nas mesmas condicoes. Mudanca de RSSI por posicao nao e mudanca de servico GATT.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Fuzzing pode travar o alvo; use apenas dispositivo proprio e reinicializavel."
    ),
    WIKI_ENTRY(
        BLE,
        "Cadeia universal",
        "Encadeia rotinas experimentais HFP, HID e FastPair contra o alvo selecionado.",
        "Executa uma avaliacao ampla de robustez em ambiente controlado.",
        "Situacao: Revisar uma sequencia experimental no prototipo.\n"
        "1. Prepare logs e recuperacao na bancada isolada.\n"
        "2. Acompanhe as etapas previstas no ensaio aprovado.\n"
        "3. Confira o funcionamento normal ao final.\n"
        "Resultado: Registre em qual etapa ocorreu a mudanca. O nome universal nao garante compatibilidade com todo dispositivo.",
        "Bluetooth LE integrado do ESP32-S3.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        BLE,
        "Captura BLE",
        "Faz scan ativo por dez segundos, guarda anuncios, nome, MAC, RSSI e payload e permite salvar.",
        "Registra publicidade BLE para diagnostico; nao captura todos os pacotes brutos.",
        "Situacao: Comparar os anuncios do seu sensor.\n"
        "1. Execute a captura com o sensor ligado.\n"
        "2. Salve o resultado.\n"
        "3. Altere uma configuracao conhecida do sensor e repita.\n"
        "Resultado: Compare nome, payload e UUIDs recebidos. A captura registra anuncios observados, nao todo o trafego BLE.",
        "BLE ESP32-S3, microSD ou LittleFS.",
        "Identificadores podem ser pessoais. Capture somente no ambiente autorizado."
    ),
    WIKI_ENTRY(
        BLE,
        "Ninebot",
        "Conecta a um dispositivo com servico Nordic UART e envia um payload fixo escolhido por modelo.",
        "Ferramenta experimental para testar um patinete proprio; o efeito exato nao e documentado no codigo.",
        "Situacao: Avaliar compatibilidade de um controlador de bancada.\n"
        "1. Confira modelo e servico Nordic UART na documentacao do seu controlador.\n"
        "2. Analise os logs do caso aprovado em bancada imobilizada.\n"
        "Resultado: O efeito do payload fixo nao e documentado no codigo. Nao interprete conexao BLE como confirmacao de funcionamento seguro.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Use somente no proprio equipamento e esteja preparado para desliga-lo com seguranca."
    ),
    WIKI_ENTRY(
        BLE,
        "Apresentador",
        "Emula controle HID BLE de slides com setas, contador e cronometro.",
        "Avanca ou retorna slides em um computador pareado.",
        "Situacao: Avancar slides no computador proprio.\n"
        "1. Pareie o MaliOS.\n"
        "2. Abra sua apresentacao em tela cheia.\n"
        "3. Envie avancar e voltar.\n"
        "Resultado: O slide deve mudar no aplicativo em foco. O contador local nao confirma o numero real do slide no computador.",
        "Bluetooth LE integrado do ESP32-S3.",
        "Pareie somente com o computador que deve receber os comandos."
    ),

    // RF / Sub-GHz
    WIKI_ENTRY(
        SUB_GHZ,
        "Procurar/copiar",
        "Recebe sinais na faixa configurada, tenta decodificar ou guardar RAW e oferece repetir e salvar.",
        "Analisa e reproduz um controle ou sensor compativel sob seu controle.",
        "Situacao: Registrar um controle de bancada proprio.\n"
        "1. Configure a frequencia conhecida do controle.\n"
        "2. Inicie a recepcao e pressione um botao uma vez.\n"
        "3. Confira o sinal e salve a captura.\n"
        "Resultado: Pode aparecer protocolo reconhecido ou RAW. Capturar nao garante que o receptor aceite repeticao, especialmente com codigo variavel.",
        "CC1101, ESP32 RMT, microSD ou LittleFS ao salvar.",
        WARN_RADIO
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Gravar RAW",
        "Grava os tempos HIGH e LOW brutos e permite repetir, salvar ou descartar a captura.",
        "Preserva sinais que nao foram reconhecidos por um decodificador.",
        "Situacao: Observar um sensor RF proprio nao reconhecido.\n"
        "1. Ajuste a frequencia do sensor.\n"
        "2. Grave enquanto ele envia uma amostra.\n"
        "3. Salve para consultar os tempos.\n"
        "Resultado: RAW registra duracoes dos pulsos. Ruido tambem pode gerar pulsos; compare varias amostras antes de interpretar o sinal.",
        "CC1101, ESP32 RMT, microSD ou LittleFS.",
        WARN_RADIO
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "SubGHz personalizado",
        "Carrega arquivos .sub de Recentes, SD ou LittleFS e transmite o sinal escolhido.",
        "Reproduz capturas compativeis ja armazenadas.",
        "Situacao: Testar uma captura do seu transmissor de bancada.\n"
        "1. Abra um .sub proprio pelo armazenamento.\n"
        "2. Confira a frequencia e os dados antes do ensaio autorizado.\n"
        "3. Observe a resposta do receptor de teste.\n"
        "Resultado: O envio nao comprova recepcao. Protocolo, modulacao e mecanismo de codigo precisam ser compativeis.",
        "CC1101, microSD ou LittleFS.",
        WARN_RADIO
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Espectro",
        "Desenha a atividade e a forma dos pulsos recebidos na frequencia selecionada.",
        "Ajuda a observar modulacao temporal; nao e uma FFT de toda a banda.",
        "Situacao: Comparar pulsos de dois controles proprios.\n"
        "1. Use a frequencia conhecida de cada controle.\n"
        "2. Acione cada um separadamente na bancada.\n"
        "3. Compare o desenho temporal.\n"
        "Resultado: Mudancas mostram atividade de pulsos recebidos, sem identificar automaticamente o modelo ou o fabricante.",
        "CC1101 GDO0 e ESP32 RMT.",
        "Recepcao passiva; respeite a privacidade de sinais proximos."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Espectro RSSI",
        "Mede RSSI ao longo do tempo ou compara frequencias dentro da faixa configurada.",
        "Mostra onde ha maior energia recebida pelo CC1101.",
        "Situacao: Comparar sinal em duas posicoes.\n"
        "1. Fixe a frequencia do seu transmissor de teste.\n"
        "2. Anote o RSSI perto e mais longe, mantendo antena e orientacao.\n"
        "3. Repita a observacao.\n"
        "Resultado: Por exemplo, -50 dBm e mais forte que -80 dBm. Os numeros nao fornecem distancia exata nem identificam a fonte.",
        "CC1101 integrado.",
        "Medicao passiva; o RSSI nao identifica sozinho a origem do sinal."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Espectro quadrado",
        "Captura os tempos HIGH e LOW e desenha a forma quadrada dos pulsos.",
        "Facilita a inspecao visual do ritmo de um sinal digital recebido.",
        "Situacao: Inspecionar o ritmo do sensor proprio.\n"
        "1. Ajuste a frequencia.\n"
        "2. Capture uma emissao do sensor.\n"
        "3. Compare os trechos altos e baixos.\n"
        "Resultado: Pulsos de larguras diferentes ficam visiveis. A tela auxilia a observacao; nao substitui medicao temporal calibrada.",
        "CC1101 GDO0 e ESP32 RMT.",
        "Recepcao passiva; use somente sinais que voce pode analisar."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Espectrograma",
        "Cria um waterfall de RSSI entre limites de frequencia ajustaveis.",
        "Mostra como a energia recebida varia por frequencia e tempo.",
        "Situacao: Observar quando o seu sensor transmite.\n"
        "1. Escolha uma faixa que inclua a frequencia conhecida.\n"
        "2. Observe com o sensor em repouso.\n"
        "3. Acione uma leitura e compare o waterfall.\n"
        "Resultado: Uma faixa mais intensa pode coincidir com a emissao. Outras fontes e a velocidade de varredura afetam a imagem.",
        "CC1101 integrado.",
        "Recepcao passiva; resultados dependem da largura e sensibilidade do CC1101."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Escutar",
        "Detecta pulsos entre 300 e 928 MHz e gera bipes correspondentes; nao demodula voz ou audio.",
        "Fornece retorno sonoro da atividade de um transmissor simples.",
        "Situacao: Perceber atividade sem olhar a tela.\n"
        "1. Ajuste uma frequencia suportada pelo modulo para o seu controle.\n"
        "2. Acione o controle proprio e ouca o retorno.\n"
        "Resultado: Os bipes acompanham pulsos detectados. Nao sao a voz, a musica ou o audio original transmitido.",
        "CC1101 e speaker NS4168.",
        "Recepcao passiva. Nao interprete o resultado como audio do transmissor."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Forca bruta",
        "Enumera codigos de protocolos suportados, com frequencia e repeticoes configuraveis.",
        "Testa um receptor proprio contra todas as combinacoes de um protocolo simples.",
        "Situacao: Avaliar um receptor descartavel em bancada blindada.\n"
        "1. Registre o comportamento normal e os logs do receptor.\n"
        "2. Compare o resultado do ensaio aprovado com esses registros.\n"
        "3. Confira a recuperacao ao terminar.\n"
        "Resultado: Avalie a defesa do receptor; ausencia de resposta nao comprova que todas as combinacoes foram recebidas.",
        "CC1101 ou transmissor RF configurado.",
        "Pode acionar dispositivos. Uso estrito em receptor proprio e laboratorio autorizado."
    ),
    WIKI_ENTRY(
        SUB_GHZ,
        "Jammer",
        "Emite interferencia nos modos potencia total, intermitente, ruido ou varredura.",
        "Testa recuperacao de um receptor em ambiente RF controlado.",
        "Situacao: Avaliar recuperacao de receptor em recinto blindado.\n"
        "1. Registre o enlace normal dos aparelhos de bancada.\n"
        "2. Compare a recepcao no ensaio aprovado.\n"
        "3. Confira o enlace ao encerrar.\n"
        "Resultado: Uma falha observada deve ser correlacionada com instrumentacao externa; o menu nao mede a interferencia produzida.",
        "CC1101 integrado.",
        "Pode interferir em servicos proximos e ser ilegal no ar. Somente bancada blindada autorizada."
    ),

    // nRF24
    WIKI_ENTRY(
        NRF24,
        "Informacoes",
        "Mostra orientacoes eticas e de cabeamento para o modulo nRF24.",
        "Lembra que fios longos e ruido podem causar falhas; nao detecta eletricamente o modulo.",
        "Situacao: Preparar um nRF24 externo.\n"
        "1. Com o aparelho desligado, confira alimentacao e pinos no esquema da sua placa.\n"
        "2. Revise fios curtos e conexoes antes de ligar.\n"
        "Resultado: A pagina orienta a montagem. Ver um pino listado nao comprova presenca fisica ou resposta eletrica do modulo.",
        "Tela; nRF24L01+ externo nas demais ferramentas.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        NRF24,
        "Espectro",
        "Le o detector RPD nos canais 0 a 79 e desenha atividade aproximada de 2,40 a 2,48 GHz.",
        "Compara ocupacao de canais; nao identifica protocolos ou dispositivos.",
        "Situacao: Comparar atividade de 2,4 GHz na bancada.\n"
        "1. Conecte o nRF24 compativel e abra o espectro.\n"
        "2. Observe os mesmos canais por algum tempo.\n"
        "3. Compare em outro local.\n"
        "Resultado: As barras representam deteccoes de energia pelo RPD, nao quantidade de redes Wi-Fi, pacotes ou aparelhos.",
        "nRF24L01+ externo via SPI.",
        "Medicao passiva; o resultado e apenas presenca de energia."
    ),
    WIKI_ENTRY(
        NRF24,
        "MouseJack",
        "Busca receptores Microsoft ou Logitech compativeis e oferece texto ou DuckyScript quando encontra alvo.",
        "Avalia se um receptor proprio e vulneravel a injecao sem fio conhecida como MouseJack.",
        "Situacao: Revisar um dongle proprio em laboratorio isolado.\n"
        "1. Identifique modelo e versao do receptor.\n"
        "2. Compare o ensaio autorizado com a orientacao de atualizacao do fabricante.\n"
        "3. Confira o resultado apos atualizar ou substituir.\n"
        "Resultado: Um receptor encontrado nao e automaticamente vulneravel. Registre apenas o comportamento confirmado no host de teste.",
        "nRF24L01+ externo; microSD ou LittleFS para scripts.",
        "Funcao ofensiva. Use somente receptor proprio e computador de teste autorizado."
    ),
    WIKI_ENTRY(
        NRF24,
        "Jammer NRF",
        "Emite portadora e alterna grupos de canais de 2,4 GHz, de forma sequencial ou aleatoria.",
        "Testa tolerancia a interferencia em equipamento controlado.",
        "Situacao: Avaliar um enlace entre dois modulos proprios blindados.\n"
        "1. Registre a comunicacao normal.\n"
        "2. Compare os logs no ensaio aprovado.\n"
        "3. Confira a retomada apos encerrar.\n"
        "Resultado: Anote a recuperacao do enlace. Nao interprete o modo escolhido como medida da potencia efetivamente recebida.",
        "nRF24L01+ externo via SPI.",
        "Interfere em Wi-Fi, BLE e outros sistemas de 2,4 GHz. Somente laboratorio blindado autorizado."
    ),

    // LoRa
    WIKI_ENTRY(
        LORA,
        "Chat",
        "Envia e recebe texto por SX1276 ou SX1262 e mantem historico em /chats.txt.",
        "Permite conversa simples entre radios LoRa configurados na mesma frequencia.",
        "Situacao: Enviar uma mensagem entre duas placas proprias.\n"
        "1. Configure os radios compativeis com a mesma frequencia e parametros.\n"
        "2. Envie ola bancada de uma placa.\n"
        "3. Confira a outra placa e o historico.\n"
        "Resultado: A mensagem recebida confirma esse envio. Sem resposta, confira modulo, antena e configuracoes em ambos os lados.",
        "Modulo LoRa externo via SPI e LittleFS; nao ha LoRa onboard neste alvo.",
        "Nao ha criptografia visivel. Use frequencia e potencia permitidas na sua regiao."
    ),

    // FM / SI4713
    WIKI_ENTRY(
        FM_RADIO,
        "Transmitir padrao",
        "Seleciona frequencia FM padrao e transmite RDS BruceRadio com a potencia configurada.",
        "Testa recepcao FM e RDS em bancada; esta funcao nao envia audio ao vivo.",
        "Situacao: Conferir RDS em receptor de bancada blindada.\n"
        "1. Prepare o SI4713 e receptor compativel no arranjo autorizado.\n"
        "2. Sintonize a frequencia do ensaio e observe o campo RDS.\n"
        "3. Use Parar transmissao ao terminar.\n"
        "Resultado: O receptor pode mostrar BruceRadio. Essa funcao nao envia audio ao vivo e RDS depende do suporte do receptor.",
        "Transmissor SI4713 externo via I2C.",
        "Transmissao FM e regulada. Use carga ficticia, blindagem ou autorizacao."
    ),
    WIKI_ENTRY(
        FM_RADIO,
        "Transmitir reservado",
        "Permite transmissao RDS na faixa aproximada de 76 a 86,9 MHz.",
        "Oferece teste de receptor em uma faixa que pode ser reservada conforme a regiao.",
        "Situacao: Validar a faixa de um receptor em recinto blindado.\n"
        "1. Confira no manual se ele sintoniza a faixa do ensaio aprovado.\n"
        "2. Compare a indicacao RDS durante o ensaio.\n"
        "3. Encerre em Parar transmissao.\n"
        "Resultado: Receptor fora da faixa pode nao sintonizar. Ausencia de RDS nao prova falha do modulo transmissor.",
        "Transmissor SI4713 externo via I2C.",
        "A faixa pode ser restrita ou ilegal. Nao transmita por antena fora de laboratorio autorizado."
    ),
    WIKI_ENTRY(
        FM_RADIO,
        "Parar transmissao",
        "Zera a potencia, reseta o SI4713 e limpa o estado de transmissao.",
        "Encerra com seguranca uma sessao de teste FM.",
        "Situacao: Encerrar a sessao FM de bancada.\n"
        "1. Escolha Parar transmissao.\n"
        "2. Confira o estado na tela e no receptor.\n"
        "3. Desligue antes de alterar a ligacao do modulo.\n"
        "Resultado: A potencia e zerada e o SI4713 reiniciado. Um nome RDS antigo pode permanecer na memoria do receptor.",
        "Transmissor SI4713 externo via I2C.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        FM_RADIO,
        "Espectro FM",
        "Mede e desenha o nivel de ruido retornado pelo SI4713 na frequencia escolhida.",
        "Ajuda a comparar uma frequencia; nao e waterfall de toda a banda.",
        "Situacao: Comparar duas frequencias do ensaio.\n"
        "1. Leia o nivel na primeira frequencia.\n"
        "2. Mude para a segunda mantendo a montagem.\n"
        "3. Repita as leituras.\n"
        "Resultado: Compare os niveis retornados pelo chip. A funcao nao mostra simultaneamente toda a faixa FM.",
        "SI4713 externo via I2C.",
        "Medicao passiva. O valor nao identifica a estacao ou o conteudo."
    ),
    WIKI_ENTRY(
        FM_RADIO,
        "Sequestrar TA",
        "Transmite RDS BruceTraffic em 107,7 MHz com o bit Traffic Announcement ativo.",
        "Demonstra como receptores proprios reagem a um anuncio de transito RDS.",
        "Situacao: Conferir TA em radio descartavel blindado.\n"
        "1. Registre a configuracao TA do receptor de bancada.\n"
        "2. Compare sua resposta no ensaio aprovado.\n"
        "3. Encerre a transmissao e confira o estado normal.\n"
        "Resultado: O efeito depende de TA habilitado e do receptor. O texto RDS pode continuar exibido depois do fim.",
        "Transmissor SI4713 externo via I2C.",
        "Alto risco de interferencia. Use somente carga ficticia ou recinto totalmente blindado autorizado."
    ),

    // Infravermelho
    WIKI_ENTRY(
        IR,
        "TV-B-Gone",
        "Envia uma colecao de comandos de energia para regioes NA ou EU e codigos universais.",
        "Verifica quais codigos de energia sao aceitos por uma TV propria.",
        "Situacao: Conferir codigos de energia da sua TV.\n"
        "1. Use somente a TV propria na bancada.\n"
        "2. Escolha a regiao adequada e aponte o emissor para o receptor.\n"
        "3. Interrompa quando observar a resposta.\n"
        "Resultado: A TV pode alternar o estado de energia. Ausencia de resposta pode ser angulo, distancia ou codigo incompativel.",
        "LED IR transmissor no GPIO configurado.",
        "Pode desligar aparelhos proximos. Use somente equipamentos proprios ou autorizados."
    ),
    WIKI_ENTRY(
        IR,
        "IR personalizado",
        "Abre arquivos .ir de Recentes, SD ou LittleFS e envia um comando ou todos.",
        "Reproduz comandos infravermelhos previamente salvos.",
        "Situacao: Usar um comando salvo do seu televisor.\n"
        "1. Abra seu arquivo .ir.\n"
        "2. Selecione apenas o comando desejado, como volume.\n"
        "3. Aponte para a TV e envie.\n"
        "Resultado: A resposta depende do arquivo e do alinhamento. Energia costuma alternar estado; repetir pode desfazer o primeiro efeito.",
        "LED IR transmissor, microSD ou LittleFS.",
        "Nao controle aparelhos de terceiros sem permissao."
    ),
    WIKI_ENTRY(
        IR,
        "Ler IR",
        "Captura IR decodificado ou RAW, permite retransmitir, salvar e criar controles rapidos.",
        "Aprende comandos de um controle remoto proprio.",
        "Situacao: Guardar o volume do controle da sua TV.\n"
        "1. Inicie a leitura e aponte o controle ao receptor IR.\n"
        "2. Pressione volume uma vez.\n"
        "3. Confira protocolo ou RAW e salve.\n"
        "Resultado: O comando capturado pode ser consultado depois. Se aparecer repeticao, solte a tecla e tente uma nova captura curta.",
        "Receptor IR, LED IR transmissor, microSD ou LittleFS.",
        "A leitura e passiva, mas a repeticao transmite comandos. Use aparelhos proprios."
    ),
    WIKI_ENTRY(
        IR,
        "Jammer IR",
        "Emite padroes Basic, Enhanced, Sweep, Random ou Empty em portadora ajustavel.",
        "Testa como um receptor IR proprio se recupera de sinais concorrentes.",
        "Situacao: Avaliar receptor IR de prototipo em bancada isolada.\n"
        "1. Registre a recepcao normal do controle de teste.\n"
        "2. Compare os erros no ensaio aprovado.\n"
        "3. Encerre e confira a recepcao normal.\n"
        "Resultado: Anote perdas e recuperacao. Nao aplique o teste perto de equipamentos de terceiros.",
        "LED IR transmissor.",
        "Pode bloquear controles proximos. Use somente ambiente controlado autorizado."
    ),

    // Ethernet
    WIKI_ENTRY(
        ETHERNET,
        "Procurar hosts",
        "Inicializa o W5500, percorre a sub-rede com ARP e lista IP, MAC e gateway.",
        "Inventaria uma rede Ethernet autorizada.",
        "Situacao: Localizar uma placa no switch de bancada.\n"
        "1. Confira W5500, cabo e configuracao de rede.\n"
        "2. Execute a busca na LAN autorizada.\n"
        "3. Compare os MACs com seu inventario.\n"
        "Resultado: A lista mostra hosts que responderam. Ausencia pode indicar enlace, isolamento ou host sem resposta ARP.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Info do host",
        "Mostra fabricante do MAC e tenta uma lista fixa de portas TCP no host selecionado.",
        "Ajuda a reconhecer e diagnosticar um equipamento encontrado.",
        "Situacao: Conferir um servidor ligado por cabo.\n"
        "1. Escolha o IP correto na lista Ethernet.\n"
        "2. Abra Info do host.\n"
        "3. Compare as portas com os servicos habilitados no servidor.\n"
        "Resultado: Porta sem resposta pode estar filtrada. O fabricante inferido pelo MAC nao confirma modelo nem proprietario.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Conectar via SSH",
        "Abre o cliente SSH para o host, mas esta acao usa Wi-Fi, mesmo iniciada na lista Ethernet.",
        "Permite uma sessao autorizada no host encontrado.",
        "Situacao: Administrar um host encontrado pelo W5500.\n"
        "1. Garanta tambem uma conexao Wi-Fi com rota ate esse host.\n"
        "2. Escolha SSH e use sua conta de teste.\n"
        "3. Consulte hostname e encerre com exit.\n"
        "Resultado: Nesta acao o SSH usa Wi-Fi. Encontrar o host por Ethernet nao garante que a sessao SSH consiga alcanca-lo.",
        "Wi-Fi ESP32-S3.",
        "Use somente servidor e credenciais autorizados."
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Desautenticar estacao",
        "Usa o radio Wi-Fi para enviar deauth ao host quando a conexao Wi-Fi esta disponivel.",
        "Testa protecao Wi-Fi de uma estacao propria encontrada.",
        "Situacao: Conferir o transporte usado pelo ensaio de bancada.\n"
        "1. Identifique o cliente Wi-Fi proprio e seu AP.\n"
        "2. Compare logs no ensaio aprovado, mesmo tendo aberto o item na lista Ethernet.\n"
        "Resultado: Esta acao depende do radio Wi-Fi. Ela nao desconecta um enlace Ethernet pelo cabo.",
        "Wi-Fi ESP32-S3; nao usa os quadros Ethernet do W5500.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Falsificacao ARP",
        "Envia respostas ARP falsas entre alvo e gateway, grava PCAP e tenta restaurar a tabela ao parar.",
        "Testa protecoes contra ARP spoofing numa LAN controlada.",
        "Situacao: Validar um monitor ARP na LAN isolada.\n"
        "1. Registre os mapeamentos legitimos dos hosts.\n"
        "2. Compare alertas e PCAP do ensaio aprovado.\n"
        "3. Ao parar, confira novamente o gateway nos hosts.\n"
        "Resultado: Valide a restauracao externamente. Um PCAP e uma observacao do ensaio, nao garantia de recuperacao da rede.",
        "W5500 externo, microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Envenenamento ARP",
        "Percorre hosts e anuncia mapeamentos ARP e MAC aleatorios, com registro PCAP.",
        "Submete uma rede descartavel a um teste agressivo de tabela ARP.",
        "Situacao: Avaliar isolamento de hosts de bancada.\n"
        "1. Guarde configuracao e tabelas ARP normais.\n"
        "2. Compare os logs defensivos no ensaio aprovado.\n"
        "3. Confira a conectividade ao encerrar.\n"
        "Resultado: Registre a protecao que atuou no switch ou host, sem inferir a causa apenas pela perda de acesso.",
        "W5500 externo, microSD ou LittleFS.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Esgotar DHCP",
        "Transmite DHCP Discover com MACs aleatorios ate o usuario interromper.",
        "Testa protecao contra consumo do pool DHCP.",
        "Situacao: Revisar alertas de um DHCP descartavel.\n"
        "1. Anote o pool livre da LAN isolada.\n"
        "2. Compare com os logs do ensaio aprovado.\n"
        "3. Ao encerrar, confira uma nova concessao para sua placa.\n"
        "Resultado: O teste so esta recuperado quando o servidor volta a atender a placa. Confira o estado no servidor.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),
    WIKI_ENTRY(
        ETHERNET,
        "Inundacao MAC",
        "Envia quadros com MAC e IP aleatorios para pressionar a tabela CAM do switch.",
        "Testa limites e alertas de um switch controlado.",
        "Situacao: Avaliar limites de um switch de bancada.\n"
        "1. Registre o estado das portas e da tabela MAC.\n"
        "2. Compare durante o ensaio aprovado.\n"
        "3. Confira trafego normal ao terminar.\n"
        "Resultado: Os logs do switch ajudam a separar bloqueio por politica de falha de enlace ou saturacao.",
        "W5500 externo via SPI.",
        WARN_AUTH_NETWORK
    ),

    // GPS
    WIKI_ENTRY(
        GPS,
        "Wardriving",
        "Associa scans Wi-Fi, BLE ou ambos a coordenadas GPS e grava CSV no formato WiGLE.",
        "Mapeia cobertura e inventario de radios em uma area autorizada.",
        "Situacao: Mapear cobertura dos seus APs em area autorizada.\n"
        "1. Aguarde coordenadas validas do GPS.\n"
        "2. Escolha Wi-Fi, BLE ou ambos e percorra os pontos de teste.\n"
        "3. Encerre e consulte o CSV.\n"
        "Resultado: Cada registro associa observacao e posicao recebida. Sem fix GPS confiavel, nao trate o ponto como localizacao precisa do transmissor.",
        "GPS UART externo, Wi-Fi e BLE ESP32-S3, microSD ou LittleFS.",
        "Coleta MACs e localizacao. Respeite privacidade e use apenas levantamento autorizado."
    ),
    WIKI_ENTRY(
        GPS,
        "Rastreador GPS",
        "Le coordenadas, acumula distancia e grava uma trilha GPX em /BruceGPS.",
        "Registra o percurso do proprio aparelho.",
        "Situacao: Registrar uma caminhada propria.\n"
        "1. Em local aberto, aguarde coordenadas validas.\n"
        "2. Inicie a trilha e caminhe.\n"
        "3. Encerre e abra o GPX de /BruceGPS em um visualizador.\n"
        "Resultado: O trajeto acompanha as amostras recebidas. Saltos podem ocorrer por sinal fraco e afetam a distancia acumulada.",
        "GPS UART externo, microSD ou LittleFS.",
        "O arquivo revela localizacao. Proteja-o e obtenha consentimento quando necessario."
    ),

    // NFC / RFID
    WIKI_ENTRY(
        RFID,
        "Ler tag",
        "Le tipo, UID, ATQA, SAK e dados disponiveis e oferece verificar, salvar, clonar, gravar ou emular conforme suporte.",
        "Inspeciona uma tag propria e prepara operacoes compativeis.",
        "Situacao: Identificar uma etiqueta NFC propria.\n"
        "1. Abra Ler tag com leitor compativel.\n"
        "2. Aproxime uma unica etiqueta.\n"
        "3. Confira tipo e UID e salve o que foi lido.\n"
        "Resultado: UID e dados disponiveis aparecem conforme suporte e permissoes. Ler UID nao significa ler toda a memoria.",
        "Modulo RFID selecionado; padrao PN532 I2C integrado; armazenamento.",
        "UIDs e dumps podem ser credenciais. Use somente tags proprias ou autorizadas."
    ),
    WIKI_ENTRY(
        RFID,
        "Ler EMV",
        "Consulta dados contactless disponiveis, como AID, emissor, PAN e datas, e pode salvar o resultado.",
        "Demonstra quais dados um cartao EMV proprio expoe por aproximacao.",
        "Situacao: Examinar um cartao contactless de teste proprio.\n"
        "1. Use um cartao de laboratorio.\n"
        "2. Aproxime-o do leitor e confira os campos retornados.\n"
        "3. Evite salvar se nao precisar do registro.\n"
        "Resultado: A consulta nao realiza pagamento. Campos podem faltar; PAN e outros dados exibidos devem permanecer privados.",
        "PN532 por I2C ou SPI e armazenamento.",
        "Dados financeiros sao sensiveis. Use somente cartao proprio e com consentimento."
    ),
    WIKI_ENTRY(
        RFID,
        "Ler 125kHz",
        "Recebe pela UART um pacote de leitor 125 kHz, valida checksum e pode salvar .rfidlf.",
        "Le o identificador de uma tag LF; esta funcao nao clona nem grava.",
        "Situacao: Inventariar uma tag LF de bancada.\n"
        "1. Conecte o leitor UART 125 kHz adequado.\n"
        "2. Aproxime a tag e aguarde leitura valida.\n"
        "3. Salve seu identificador.\n"
        "Resultado: Um PN532 NFC nao substitui esse leitor LF. Esta funcao recebe um identificador e nao grava uma copia da tag.",
        "Leitor RFID 125 kHz externo via UART.",
        "Identificadores podem controlar acesso. Leia apenas tags proprias ou autorizadas."
    ),
    WIKI_ENTRY(
        RFID,
        "Ferramenta SRIX",
        "Le UID e dump de SRIX4K ou SRIX512, salva e carrega .srix e pode escrever ou clonar o dump.",
        "Faz manutencao e pesquisa de tags SRIX compativeis.",
        "Situacao: Fazer backup de uma tag SRIX propria.\n"
        "1. Use tag e leitor compativeis.\n"
        "2. Leia UID e dump.\n"
        "3. Salve o .srix e confira o arquivo antes de qualquer alteracao.\n"
        "Resultado: Backup depende dos dados efetivamente lidos. Um bloco protegido pode impedir leitura ou escrita completa.",
        "PN532 I2C integrado; antena desta placa pode ter alcance limitado.",
        "Gravacao pode corromper a tag. Use tag propria, backup e autorizacao."
    ),
    WIKI_ENTRY(
        RFID,
        "Procurar tags",
        "Registra continuamente UIDs unicos encontrados e salva o resultado ao sair.",
        "Cria um inventario rapido de tags presentes numa bancada.",
        "Situacao: Inventariar tres etiquetas de laboratorio.\n"
        "1. Inicie a busca.\n"
        "2. Apresente uma etiqueta por vez e repita uma delas.\n"
        "3. Saia para salvar.\n"
        "Resultado: UID repetido nao deve criar um novo UID unico. Etiquetas diferentes com o mesmo UID nao sao distinguiveis por esse criterio.",
        "Modulo RFID selecionado e armazenamento.",
        "Inventarie apenas tags proprias ou autorizadas."
    ),
    WIKI_ENTRY(
        RFID,
        "Carregar arquivo",
        "Carrega um dump RFID e oferece verificar, gravar, clonar UID ou emular conforme o driver.",
        "Restaura ou usa um backup criado pelo proprio usuario.",
        "Situacao: Consultar um backup RFID proprio.\n"
        "1. Selecione o dump salvo.\n"
        "2. Confira o tipo e dados antes de escolher uma operacao.\n"
        "3. Use uma tag descartavel compativel se for testar escrita.\n"
        "Resultado: Carregar o arquivo nao garante suporte a todas as acoes. UID fixo ou memoria protegida podem impedir a escrita.",
        "Modulo RFID, microSD ou LittleFS.",
        "O dump pode ser credencial e as acoes podem alterar tags. Laboratorio autorizado."
    ),
    WIKI_ENTRY(
        RFID,
        "Apagar dados",
        "Chama a operacao de apagamento do driver sobre a tag apresentada.",
        "Limpa uma tag gravavel quando o modulo e o tipo suportam a acao.",
        "Situacao: Limpar uma tag descartavel de teste.\n"
        "1. Faca e confira um backup.\n"
        "2. Apresente somente a tag que sera apagada.\n"
        "3. Execute e releia a tag para verificar.\n"
        "Resultado: A verificacao mostra o que realmente mudou. Apagamento depende do driver, tipo de tag e permissoes de memoria.",
        "Modulo RFID selecionado.",
        "Acao destrutiva. Use somente tag propria, descartavel ou com backup."
    ),
    WIKI_ENTRY(
        RFID,
        "Emular NDEF",
        "Cria NDEF de texto, URL, Wi-Fi ou link salvo e coloca modulo compativel em modo alvo.",
        "Apresenta conteudo NDEF a um leitor proximo sem gravar uma etiqueta.",
        "Situacao: Mostrar texto ao celular proprio.\n"
        "1. Escolha NDEF de texto e informe Ola MaliOS.\n"
        "2. Inicie a emulacao no modulo compativel.\n"
        "3. Aproxime o celular com NFC ativo.\n"
        "Resultado: Um leitor compativel mostra o texto sem gravar uma etiqueta fisica. Nem todo telefone reage da mesma maneira.",
        "PN532 ou ST25R compativel.",
        "O conteudo fica disponivel a leitores proximos. Nao use dados sensiveis."
    ),
    WIKI_ENTRY(
        RFID,
        "Gravar NDEF",
        "Cria NDEF de texto, URL, Wi-Fi ou link e tenta grava-lo em tag compativel.",
        "Prepara uma etiqueta NFC com conteudo simples.",
        "Situacao: Criar uma etiqueta de inventario propria.\n"
        "1. Escolha texto e informe Caixa 01.\n"
        "2. Aproxime uma tag gravavel compativel.\n"
        "3. Depois da gravacao, leia-a no celular.\n"
        "Resultado: O leitor deve mostrar Caixa 01. Se falhar, confira espaco, protecao contra escrita e suporte ao tipo de tag.",
        "Modulo RFID e tag gravavel; suporte principal a MIFARE Ultralight.",
        "Pode sobrescrever o conteudo. Use tag propria e mantenha backup."
    ),
    WIKI_ENTRY(
        RFID,
        "Amiibolink",
        "Conecta por BLE ao Amiibolink, envia dump NTAG215 valido e ajusta o modo de UID.",
        "Gerencia um acessorio Amiibolink externo com backups compativeis.",
        "Situacao: Conferir um backup proprio no acessorio.\n"
        "1. Pareie o Amiibolink compativel.\n"
        "2. Selecione um dump NTAG215 legitimo do seu arquivo.\n"
        "3. Confira o resultado no acessorio.\n"
        "Resultado: Arquivo aceito nao certifica funcionamento em todo leitor. Mantenha o backup original antes de trocar o conteudo do acessorio.",
        "BLE ESP32-S3, Amiibolink externo e armazenamento.",
        "Use somente backups proprios e respeite licencas, regras e direitos aplicaveis."
    ),
    WIKI_ENTRY(
        RFID,
        "Chameleon",
        "Controla Chameleon Ultra por BLE para ler, buscar, salvar, gravar e emular tags HF ou LF.",
        "Centraliza operacoes de laboratorio com o periferico Chameleon.",
        "Situacao: Inventariar uma tag com Chameleon Ultra proprio.\n"
        "1. Conecte ao periferico correto por BLE.\n"
        "2. Escolha leitura HF ou LF conforme sua tag.\n"
        "3. Leia e salve o resultado.\n"
        "Resultado: Confira UID e tipo antes de qualquer escrita ou emulacao. As capacidades dependem do periferico e seu firmware.",
        "BLE ESP32-S3, Chameleon Ultra externo e armazenamento.",
        "Clonar, gravar ou emular somente tags proprias ou autorizadas. Reset de fabrica apaga dados."
    ),
    WIKI_ENTRY(
        RFID,
        "PN532 BLE",
        "Controla PN532 BLE ou PN532Killer externo para leitura, dump, escrita e emulacao compativeis.",
        "Usa um leitor NFC remoto em ensaios de bancada.",
        "Situacao: Ler uma etiqueta usando leitor externo proprio.\n"
        "1. Conecte ao PN532 BLE compativel.\n"
        "2. Apresente sua tag de teste.\n"
        "3. Confira UID e salve o dump disponivel.\n"
        "Resultado: A conexao BLE nao prova leitura NFC. Confira o retorno da tag e os blocos efetivamente recebidos.",
        "BLE ESP32-S3 e PN532 BLE ou PN532Killer externo.",
        "Dumps, gravacao e emulacao somente em tags proprias ou autorizadas."
    ),
    WIKI_ENTRY(
        RFID,
        "PN532 UART",
        "Controla PN532 ou PN532Killer por UART; no Killer inclui emulacao, sniffers e pontes BLE/TCP/UDP.",
        "Opera e diagnostica um leitor NFC serial externo.",
        "Situacao: Conferir comunicacao do leitor serial.\n"
        "1. Revise a ligacao UART com o aparelho desligado.\n"
        "2. Ligue e inicie a leitura pelo modulo.\n"
        "3. Apresente uma tag propria conhecida.\n"
        "Resultado: UID correto confirma uma leitura. Recursos PN532Killer nao estao necessariamente presentes num PN532 comum.",
        "UART, PN532 externo e opcionalmente BLE, Wi-Fi e armazenamento.",
        "Sniffing, ponte e emulacao apenas em laboratorio proprio; nao exponha a ponte a redes nao confiaveis."
    ),

    // Arquivos e compartilhamento
    WIKI_ENTRY(
        FILES,
        "Cartao SD",
        "Abre o gerenciador do microSD para navegar, ver informacoes, renomear, copiar, excluir e abrir arquivos suportados.",
        "Gerencia arquivos grandes e dados removiveis usados pelas ferramentas.",
        "Situacao: Guardar uma copia de captura propria.\n"
        "1. Abra Cartao SD e localize o arquivo.\n"
        "2. Copie para outra pasta pelo menu do arquivo.\n"
        "3. Confira nome e tamanho no destino.\n"
        "Resultado: A copia deve permanecer acessivel sem alterar o original. Se o SD nao aparecer, confira montagem e encaixe com o aparelho em repouso.",
        "Cartao microSD e barramento SPI.",
        "Excluir e sobrescrever sao acoes reais. Ejete o cartao e mantenha backup de dados importantes."
    ),
    WIKI_ENTRY(
        FILES,
        "LittleFS",
        "Abre o mesmo gerenciador de arquivos na particao interna LittleFS.",
        "Gerencia configuracoes, scripts e capturas na flash quando nao ha microSD.",
        "Situacao: Localizar um registro sem microSD.\n"
        "1. Abra LittleFS.\n"
        "2. Procure a pasta usada pela ferramenta, como /MaliKeys/.\n"
        "3. Confira o arquivo salvo.\n"
        "Resultado: SD e LittleFS sao armazenamentos diferentes. Pasta ausente pode indicar que o registro foi salvo no SD.",
        "Flash interna do ESP32-S3.",
        "O espaco e limitado. Exclusoes e sobrescritas nao sao desfeitas automaticamente."
    ),
    WIKI_ENTRY(
        FILES,
        "Enviar arquivo",
        "Descobre outro Bruce ou MaliOS e envia um arquivo em blocos por ESP-NOW.",
        "Transfere dados localmente entre dois dispositivos compativeis.",
        "Situacao: Transferir uma captura entre duas placas suas.\n"
        "1. Na segunda placa, abra Receber arquivo.\n"
        "2. Na primeira, escolha o arquivo e o destino encontrado.\n"
        "3. Aguarde o termino e confira o arquivo recebido.\n"
        "Resultado: Compare nome e tamanho em ambas. Falha de descoberta pede conferir modo de recepcao, compatibilidade e proximidade.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        "Envie somente a peer autorizado e confira se o arquivo nao contem dados sensiveis."
    ),
    WIKI_ENTRY(
        FILES,
        "Receber arquivo",
        "Aguarda transferencia ESP-NOW e grava o arquivo no armazenamento escolhido sem substituir nome existente.",
        "Recebe dados de outro dispositivo Bruce ou MaliOS.",
        "Situacao: Receber um arquivo conhecido da outra placa.\n"
        "1. Escolha o armazenamento de destino.\n"
        "2. Inicie a recepcao.\n"
        "3. Envie pela sua segunda placa e confira o arquivo ao terminar.\n"
        "Resultado: Nome ja existente nao e substituido. Organize o destino ou use outro nome antes de repetir a transferencia.",
        "Wi-Fi ESP32-S3, microSD ou LittleFS.",
        "Aceite arquivos somente de origem confiavel e revise-os antes de executar."
    ),
    WIKI_ENTRY(
        FILES,
        "Enviar comandos",
        "Envia comandos do console Bruce para outro dispositivo por ESP-NOW.",
        "Administra uma segunda placa propria sem cabo serial.",
        "Situacao: Consultar uma segunda placa propria.\n"
        "1. Habilite Receber comandos na placa de bancada.\n"
        "2. Na origem, selecione o destino correto e envie um comando de consulta, como info.\n"
        "3. Confira a serial da placa receptora.\n"
        "Resultado: O comando e executado no destino. Nao presuma que a resposta aparecera na tela da placa que enviou.",
        "Wi-Fi integrado do ESP32-S3.",
        "Comandos podem alterar o aparelho remoto. Use somente peer proprio e autorizado."
    ),
    WIKI_ENTRY(
        FILES,
        "Receber comandos",
        "Aguarda comandos ESP-NOW e os entrega ao interpretador de comandos do firmware.",
        "Permite controle remoto entre dispositivos Bruce ou MaliOS confiaveis.",
        "Situacao: Permitir uma consulta remota na bancada.\n"
        "1. Ative a recepcao somente na placa destinada ao teste.\n"
        "2. Envie info pela sua outra placa.\n"
        "3. Confira a serial da receptora e encerre esse modo.\n"
        "Resultado: A recepcao entrega o comando ao interpretador local. Manter esse modo aberto permite novos comandos dos emissores compativeis.",
        "Wi-Fi integrado do ESP32-S3.",
        "Quem envia pode acionar funcoes do aparelho. Ative somente em laboratorio e com peer confiavel."
    ),
    WIKI_ENTRY(
        FILES,
        "Armazenamento USB",
        "Usa a implementacao original do Bruce para expor os setores brutos do microSD ao computador por USB MSC.",
        "Permite copiar arquivos do cartao como em uma unidade USB; nao expoe LittleFS nesta build.",
        "Situacao: Copiar uma captura do microSD para o PC.\n"
        "1. Conecte um cabo USB de dados e inicie o modo.\n"
        "2. Copie o arquivo na unidade apresentada.\n"
        "3. Ejete a unidade no PC antes de sair do modo.\n"
        "Resultado: A unidade expoe o SD. Arquivos do LittleFS precisam de outro acesso, como a WebUI.",
        "USB nativa do ESP32-S3 e cartao microSD.",
        "Ejete com seguranca. Remover cabo ou cartao durante escrita pode corromper o sistema de arquivos."
    ),

    // Scripts JavaScript
    WIKI_ENTRY(
        SCRIPTS,
        "Executar script JS",
        "Lista .js e .bjs encontrados nas pastas suportadas ou permite carregar outro arquivo e roda o motor mJS.",
        "Automatiza funcoes expostas pelo interpretador do Bruce.",
        "Situacao: Executar uma demonstracao sua.\n"
        "1. Revise um .js ou .bjs conhecido e compativel com as APIs mJS do firmware.\n"
        "2. Abra o arquivo na lista de scripts.\n"
        "3. Confira a mensagem ou efeito previsto.\n"
        "Resultado: JavaScript de navegador ou Node.js pode usar APIs ausentes. Se falhar, confira a mensagem do interpretador e as APIs usadas.",
        "ESP32-S3, microSD ou LittleFS; hardware adicional depende do script.",
        "Script e codigo: pode acessar arquivos, rede e hardware. Execute somente conteudo confiavel e revisado."
    ),

    // Relogio
    WIKI_ENTRY(
        CLOCK,
        "Relogio",
        "Mostra a hora de software atualizada a cada segundo; esta placa nao possui RTC fisico definido.",
        "Consulta a hora configurada ou sincronizada no firmware.",
        "Situacao: Usar o MaliOS como relogio de bancada.\n"
        "1. Configure ou sincronize a hora.\n"
        "2. Abra Relogio e compare com uma referencia.\n"
        "3. Confira novamente apos reiniciar.\n"
        "Resultado: A hora depende da configuracao de software. Esta placa nao tem RTC fisico definido que garanta hora correta sem sincronizacao.",
        "ESP32-S3 e tela.",
        "A hora pode se perder ou desviar sem sincronizacao; nao use como referencia critica."
    ),
    WIKI_ENTRY(
        CLOCK,
        "Temporizador",
        "Configura uma contagem regressiva de ate 99:59:59 e toca o som escolhido ao terminar.",
        "Lembra o fim de uma atividade simples.",
        "Situacao: Lembrar o fim de cinco minutos de bancada.\n"
        "1. Configure 00:05:00.\n"
        "2. Escolha um som e inicie a contagem.\n"
        "3. Observe o fim da contagem.\n"
        "Resultado: O aviso deve tocar pelo alto-falante disponivel. Confira antes volume e audibilidade se for depender do lembrete.",
        "Tela, encoder e speaker NS4168 por I2S.",
        "Nao e temporizador certificado para processos de seguranca ou tempo critico."
    ),

    // Ferramentas e aplicativos
    WIKI_ENTRY(
        OTHERS,
        "D20",
        "Simula dados D4, D6, D8, D10, D12, D20 e D100 com rolagem rapida e historico.",
        "Fornece sorteios simples para jogos e testes.",
        "Situacao: Resolver uma jogada de RPG.\n"
        "1. Escolha D20.\n"
        "2. Role e leia o resultado.\n"
        "3. Consulte o historico e role novamente.\n"
        "Resultado: Cada resultado vai de 1 a 20. Repetir um numero e normal e nao indica que o dado travou.",
        "Gerador aleatorio do ESP32-S3, tela e encoder.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        OTHERS,
        "Mini Pixel Paint",
        "Editor monocromatico 16 por 16 que pinta, apaga, limpa, salva, carrega e exporta header.",
        "Cria pequenos sprites diretamente no dispositivo.",
        "Situacao: Desenhar um icone simples de cruz.\n"
        "1. Pinte uma linha horizontal e uma vertical na grade 16 por 16.\n"
        "2. Use Apagar para corrigir um pixel.\n"
        "3. Salve e depois use Carregar.\n"
        "Resultado: O desenho salvo deve voltar. Exportar bitmap gera /MaliPaint/paint_001.h; o editor e monocromatico e usa um slot fixo.",
        "Tela, encoder, microSD ou LittleFS.",
        "O slot de pintura e fixo; salve uma copia antes de sobrescrever trabalho importante."
    ),
    WIKI_ENTRY(
        OTHERS,
        "QRCodes",
        "Exibe QRs salvos e cria payload PIX ou QR personalizado para mostrar, salvar ou remover.",
        "Compartilha texto ou dados codificados visualmente.",
        "Situacao: Compartilhar texto com o celular.\n"
        "1. Crie um QR personalizado com Ola MaliOS.\n"
        "2. Exiba na tela.\n"
        "3. Leia com a camera ou leitor QR do celular.\n"
        "Resultado: O leitor mostra o texto codificado. Em QR PIX, confira favorecido e valor no banco; mostrar o QR nao confirma pagamento.",
        "Tela, encoder e configuracao na flash.",
        "Confira o conteudo antes de compartilhar. PIX apenas monta o payload e nao confirma pagamento."
    ),
    WIKI_ENTRY(
        OTHERS,
        "Megalodon",
        "Minijogo de tubarao e peixes com pontuacao.",
        "Oferece entretenimento local no dispositivo.",
        "Situacao: Jogar uma rodada local.\n"
        "1. Abra o jogo e observe o tubarao e os peixes.\n"
        "2. Use os controles indicados na tela.\n"
        "3. Acompanhe a pontuacao durante a rodada.\n"
        "Resultado: O retorno e local na tela. Confira o estado da partida antes de interpretar ausencia de movimento como problema do encoder.",
        "Tela, encoder e botoes.",
        NO_WARNING
    ),
    WIKI_ENTRY(
        OTHERS,
        "Espectro do microfone",
        "Captura audio I2S e desenha o espectro e o historico calculados por FFT.",
        "Visualiza a distribuicao aproximada de frequencias do som ambiente.",
        "Situacao: Observar um som de teste proprio.\n"
        "1. Em ambiente silencioso, abra o espectro.\n"
        "2. Reproduza um tom no seu alto-falante em volume baixo.\n"
        "3. Pare o tom e compare o grafico.\n"
        "Resultado: A energia deve mudar perto da frequencia do tom. Reflexoes, ruido e harmonicos podem gerar outros picos.",
        "Microfone SPM1423 integrado, I2S e tela.",
        "Respeite privacidade e consentimento ao captar conversas ou ambientes compartilhados."
    ),
    WIKI_ENTRY(
        OTHERS,
        "Gravar microfone",
        "Grava WAV mono de 48 kHz e 16 bits por duracao e ganho configurados; stealth reduz o brilho.",
        "Registra audio para teste e diagnostico local.",
        "Situacao: Conferir a captacao do microfone.\n"
        "1. Configure uma gravacao curta e armazenamento disponivel.\n"
        "2. Grave uma frase sua em volume normal.\n"
        "3. Abra o WAV salvo e ouca.\n"
        "Resultado: Som distorcido pode pedir ganho menor; som baixo pede revisar ganho e distancia. Use apenas participantes que concordaram com a gravacao.",
        "Microfone SPM1423 I2S, microSD ou LittleFS.",
        "Grave somente com consentimento. Modo stealth nao altera obrigacoes legais ou de privacidade."
    ),
    WIKI_ENTRY(
        OTHERS,
        "iButton",
        "Le UID OneWire DS1990A, valida CRC, salva ou carrega .ibtn e grava chaves RW1990 compativeis.",
        "Faz inventario, backup e teste de chaves iButton proprias.",
        "Situacao: Inventariar um iButton de laboratorio.\n"
        "1. Conecte o probe OneWire no pino configurado.\n"
        "2. Encoste seu DS1990A e confira UID e CRC.\n"
        "3. Salve o .ibtn.\n"
        "Resultado: CRC valido indica consistencia da leitura, nao permissao de acesso. Um contato instavel pode exigir reposicionar o probe.",
        "Probe OneWire externo no GPIO configurado, microSD ou LittleFS.",
        "UID pode ser credencial de acesso. Copie somente chaves e sistemas proprios autorizados."
    ),

    // BadUSB & HID
    WIKI_ENTRY(
        USB_HID,
        "BadUSB",
        "Seleciona DuckyScript no SD ou LittleFS, cria teclado USB HID e executa o script apos confirmacao.",
        "Automatiza entradas de teclado em computador de teste.",
        "Situacao: Digitar uma frase no computador proprio.\n"
        "1. Abra um editor vazio no PC.\n"
        "2. Revise um script contendo apenas STRING Ola MaliOS.\n"
        "3. Conecte por USB de dados e confirme a execucao.\n"
        "Resultado: A frase aparece na janela em foco. Confira o layout de teclado se os caracteres forem diferentes.",
        "USB nativa ESP32-S3, microSD ou LittleFS.",
        WARN_HID
    ),
    WIKI_ENTRY(
        USB_HID,
        "Teclado USB",
        "Emula teclado USB interativo com texto, modificadores, navegacao, funcoes e fila de comandos.",
        "Permite digitar no host conectado usando a interface do T-Embed.",
        "Situacao: Escrever uma nota curta no PC proprio.\n"
        "1. Conecte por cabo de dados e abra um editor vazio.\n"
        "2. Envie Ola MaliOS.\n"
        "3. Teste uma tecla de apagar.\n"
        "Resultado: O texto deve aparecer no campo em foco. Um cabo apenas de carga nao permite teclado USB.",
        "USB nativa ESP32-S3, tela e encoder.",
        WARN_HID
    ),
    WIKI_ENTRY(
        USB_HID,
        "Clicador USB",
        "Emula mouse USB e gera cliques com intervalo, quantidade e botao configuraveis.",
        "Automatiza um ensaio repetitivo de interface em host proprio.",
        "Situacao: Testar um contador de cliques do seu aplicativo.\n"
        "1. Abra uma pagina de teste sem acoes destrutivas.\n"
        "2. Configure dez cliques esquerdos com intervalo de um segundo.\n"
        "3. Posicione o cursor no contador e inicie.\n"
        "Resultado: O contador deve aumentar dez vezes se a janela mantiver foco e aceitar todos os eventos. Pare se o cursor sair do alvo.",
        "USB nativa do ESP32-S3.",
        "Pode produzir cliques involuntarios. Use somente aplicacao e computador proprios."
    ),
    WIKI_ENTRY(
        USB_HID,
        "USB U2F",
        "Atua como autenticador USB U2F ou CTAP, com cadastro, login e presenca confirmada pelo botao.",
        "Testa autenticacao de dois fatores compativel com o firmware.",
        "Situacao: Cadastrar um autenticador em conta de laboratorio.\n"
        "1. Use uma conta que ja tenha recuperacao configurada.\n"
        "2. Inicie o cadastro de chave de seguranca no servico.\n"
        "3. Conecte o USB e confirme a presenca quando solicitado.\n"
        "Resultado: Teste o login em nova sessao mantendo a recuperacao. Compatibilidade depende do navegador, servico e implementacao do firmware.",
        "USB nativa, flash Preferences e LittleFS, tela e encoder.",
        "Valide compatibilidade e mantenha recuperacao. Nao use como unico fator em conta importante sem testes."
    ),

    // Mali Tools
    WIKI_ENTRY(
        MALI_TOOLS,
        "Info. do Sistema",
        "Mostra versoes, chip, revisao, CPU, flash, heap, PSRAM, tempo ligado, MAC e dados da compilacao.",
        "Ajuda a identificar firmware e recursos disponiveis para diagnostico.",
        "Situacao: Relatar um problema que ocorre ao abrir uma ferramenta.\n"
        "1. Anote versao, modelo e heap livre antes de abrir.\n"
        "2. Repita a consulta depois de sair da ferramenta.\n"
        "3. Inclua os passos e os dois valores no relato.\n"
        "Resultado: Heap varia durante o uso. Uma leitura menor isolada nao comprova vazamento de memoria.",
        "ESP32-S3 e tela.",
        "O endereco MAC identifica o aparelho; evite publica-lo sem necessidade."
    ),
    WIKI_ENTRY(
        MALI_TOOLS,
        "Status do Hardware",
        "Mostra estados Wi-Fi e BLE e a configuracao de pinos de CC1101, nRF24, PN532, SD e GPS.",
        "Confere se o firmware tem recursos e pinos configurados; nao e teste eletrico garantido.",
        "Situacao: Conferir um modulo antes de usa-lo.\n"
        "1. Abra o status e leia os pinos configurados.\n"
        "2. Com o aparelho desligado, compare com sua montagem.\n"
        "3. Ligue e use uma leitura conhecida para validar.\n"
        "Resultado: Configurado significa que existe configuracao no firmware; nao garante que o modulo esteja conectado ou funcionando.",
        "ESP32-S3 e os perifericos listados, quando instalados.",
        "Um status configurado nao garante que o modulo fisico esteja presente ou saudavel."
    ),
    WIKI_ENTRY(
        MALI_TOOLS,
        "Energia",
        "Mostra bateria, tensao, carga e tempo ligado usando as leituras disponiveis.",
        "Ajuda a acompanhar a alimentacao durante testes.",
        "Situacao: Acompanhar uma sessao longa de bancada.\n"
        "1. Anote bateria e tensao no inicio.\n"
        "2. Consulte novamente durante o uso e com carregador conectado.\n"
        "Resultado: Compare a tendencia das leituras disponiveis. Percentual pode variar com carga e consumo; nao e tempo restante garantido.",
        "Fuel gauge BQ27220 por I2C e ESP32-S3.",
        "Leituras sao indicativas; nao substituem instrumento de medicao para trabalho critico."
    ),
    WIKI_ENTRY(
        MALI_TOOLS,
        "Sobre o MaliOS",
        "Mostra versao, creditos, dados da compilacao e modelo do dispositivo.",
        "Identifica a distribuicao instalada e reconhece os projetos de origem.",
        "Situacao: Identificar a versao instalada.\n"
        "1. Abra Sobre o MaliOS no aparelho.\n"
        "2. Anote versao, modelo e dados de compilacao.\n"
        "3. Informe esses dados ao relatar um erro.\n"
        "Resultado: A versao mostrada vem do firmware gravado. Alterar o codigo no computador nao atualiza o aparelho sem compilar e gravar.",
        "Tela e informacoes compiladas no firmware.",
        NO_WARNING
    ),

    WIKI_ENTRY(
        MALI_TOOLS, "Chaves",
        "Catalogo de referencias de chaves planas e cruciformes, com medidas manuais em mm e quatro faces independentes.",
        "Identificar, visualizar e comparar geometrias sem calcular profundidades de corte.",
        "Situacao: Organizar referencias das suas chaves.\n"
        "1. Abra Ferramentas > Mali Tools > Chaves.\n"
        "2. Escolha Plana ou Cruciforme, informe suas medidas e notas.\n"
        "3. Salve como Bancada_A e reabra no Catalogo.\n"
        "Resultado: Os dados ficam em /MaliKeys/. Use ? Ajuda de Chaves para exemplos de medicao e comparacao; o desenho nao e escala fisica 1:1.",
        "Tela, encoder e SD ou LittleFS. Regua ou paquimetro externo para medir.",
        "O desenho e ilustrativo, sem escala fisica 1:1. Fabricante e um dado informado, nao uma identificacao automatica."
    ),
    WIKI_ENTRY(
        MALI_TOOLS, "KEY GAUGE",
        "Mantem o editor geometrico anterior, separado do novo catalogo Chaves e de seus registros.",
        "Consultar e editar os perfis relativos ja existentes, inclusive pela WebUI.",
        "Situacao: Ajustar apenas a aparencia de um perfil.\n"
        "1. Crie seis pontos, espessura 5 e largura 90%.\n"
        "2. Mude apenas espessura para 8.\n"
        "3. Mude apenas largura para 60% e salve com outro nome.\n"
        "Resultado: O corpo engrossa sem mudar os niveis nem o contorno superior; a largura reduz o comprimento visual. Nenhum valor representa mm ou modelo comercial.",
        "Tela e armazenamento em /MaliTools/KeyGauge. Wi-Fi para a WebUI.",
        "Espessura e largura sao escalas visuais independentes dos niveis. Previa Web usa RAM; nao grava automaticamente."
    ),
    WIKI_ENTRY(
        MALI_TOOLS, "Testes de resiliencia / Counter",
        "Abre o laboratorio Counter com configuracao, simulacao, observacao, metricas e historico conforme cada modulo.",
        "Acompanhar ensaios controlados e comparar resultados no aparelho ou na WebUI.",
        "Situacao: Aprender os controles sem operar os radios.\n"
        "1. Abra Counter e mantenha Simulacao ativa.\n"
        "2. Escolha duracao de 10 s e inicie.\n"
        "3. Use Parar, aguarde o estado final e escolha Salvar resultado.\n"
        "Resultado: As metricas sao artificiais. O arquivo fica em /MaliTools/Counter/; valores simulados nao descrevem a rede ou o hardware.",
        "Wi-Fi/BLE integrados; CC1101, nRF24, IR ou NFC conforme a capacidade informada.",
        "Modos ativos exigem a confirmacao de autorizacao. A simulacao nao equivale a uma transmissao real."
    ),
    WIKI_ENTRY(
        MALI_TOOLS, "Navegacao MaliOS",
        "Organiza o menu em Rede, Radio, Ferramentas, Counter, Arquivos e Sistema com cartoes e vizinhos visiveis.",
        "Manter o acesso as ferramentas anteriores usando o encoder.",
        "Situacao: Abrir e ler uma pagina de ajuda.\n"
        "1. Gire ate Ferramentas e clique.\n"
        "2. Abra ? Ajuda e escolha um tema.\n"
        "3. Gire para ler as linhas abaixo da tela; clique ou segure para voltar.\n"
        "Resultado: As setas indicam mais texto. Em editores, leia o rodape: clicar pode confirmar um valor ou avancar um ponto.",
        "Tela e encoder do T-Embed.",
        "Menus ocultados nas configuracoes continuam ocultos ate serem reativados."
    ),
    WIKI_ENTRY(
        MALI_TOOLS, "D20 e Pixel Paint",
        "D20 rola dados com historico. Pixel Paint edita uma grade de pixels com painel adaptado a orientacao da tela.",
        "Usar os aplicativos locais com a identidade visual MaliOS.",
        "Situacao: Usar dois aplicativos locais.\n"
        "1. Em D20, escolha D6 e role: o resultado fica entre 1 e 6.\n"
        "2. No Pixel Paint, pinte uma cruz na grade 16 por 16.\n"
        "3. Salve e carregue o desenho.\n"
        "Resultado: D20 mantem historico da sessao. Pixel Paint e monocromatico; use Pintar ou Apagar e copie o arquivo se quiser preservar outra versao.",
        "Tela e encoder; armazenamento para desenhos salvos.", WARN_STORAGE
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Chave Plana",
        "Registra comprimento total e util, largura, espessura, posicoes aproximadas, lado, orientacao, cabeca e canaletas.",
        "Catalogar uma chave vista de lado, com fabricante informado, tipo de perfil e observacoes.",
        "Situacao: Catalogar uma chave sua medida com paquimetro.\n"
        "1. Abra Chave Plana > Medir Chave.\n"
        "2. Exemplo didatico: total 55,00; util 28,00; largura 8,00; espessura 2,00 mm. Use suas medidas.\n"
        "3. Edite notas, visualize e salve como Bancada_A.\n"
        "Resultado: O registro guarda as medidas informadas. Hachura marca a regiao serrilhada sem profundidades de corte. O desenho nao e regua 1:1.",
        "Tela, encoder e regua ou paquimetro.",
        "A faixa hachurada marca a regiao serrilhada. Nao ha tabela de profundidades nem perfil de corte para fabricar."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Chave Cruciforme",
        "Mostra uma haste e sua secao frontal em cruz, identificando as faces A, B, C e D.",
        "Registrar por face posicoes, espacamento visual, comprimento, braco, largura, orientacao e notas.",
        "Situacao: Registrar diferencas entre as quatro faces.\n"
        "1. Defina medidas gerais antes das faces.\n"
        "2. Exemplo didatico: util 28,00 mm; face A com comprimento 25,00, braco 3,00 e largura 2,00 mm.\n"
        "3. Preencha B, C e D com suas proprias medidas.\n"
        "Resultado: Em Visualizar, girar alterna A acima, B direita, C abaixo e D esquerda. Braco vai do centro a borda; dados de A nao preenchem as outras faces.",
        "Tela e encoder; medicao manual de cada braco.",
        "Braco significa distancia do centro ate a borda externa. Espacamento de 50 a 150 por cento e apenas visual."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Medir Chave",
        "Recebe medidas manuais em milimetros. Zero representa uma medida ainda nao informada.",
        "Manter grandezas separadas e coerentes: comprimento util nao supera o total; comprimento da face nao supera o util.",
        "Situacao: Registrar uma largura medida de 8,25 mm.\n"
        "1. Em Configuracoes, use passo 1,00 para chegar perto de 8,00.\n"
        "2. Use 0,10 para chegar a 8,20 e 0,01 para 8,25.\n"
        "3. Clique confirma; segurar cancela a edicao atual.\n"
        "Resultado: Zero significa nao informado, nao medida fisica zero. Informe total antes do util e util antes das faces; o util nao pode superar o total informado.",
        "Regua ou paquimetro externo. O aparelho nao mede automaticamente.",
        "A precisao exibida e de 0,01 mm; ela nao aumenta a precisao do instrumento nem torna a tela uma regua calibrada."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Catalogo: salvar e carregar",
        "Salva registros pequenos em /MaliKeys/, no SD disponivel ao entrar ou no armazenamento interno.",
        "Consultar dados depois de reiniciar, mantendo Chaves separado dos arquivos antigos .mkg.",
        "Situacao: Guardar o registro e criar uma variante.\n"
        "1. Salve o rascunho como Bancada_A.\n"
        "2. Abra Catalogo > Bancada_A > Visualizar.\n"
        "3. Use Salvar como com Bancada_B para uma copia.\n"
        "Resultado: Voce tera dois .mkey em /MaliKeys/. O modulo usa SD disponivel ao entrar ou LittleFS; se nao achar o arquivo, confira qual armazenamento foi usado.",
        "SD ou LittleFS; formato .mkey com versao e verificacao de integridade.",
        "Limite de 256 registros. Nomes aceitam letras, numeros, espacos internos, - e _. Gravar usa arquivo temporario e copia de recuperacao."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Renomear e excluir",
        "O menu de cada registro oferece Renomear e Excluir. Excluir exige confirmacao e nomes existentes nao sao sobrescritos ao renomear.",
        "Organizar referencias sem alterar suas medidas ou os perfis do KEY GAUGE.",
        "Situacao: Organizar dois registros de teste.\n"
        "1. No Catalogo, abra Bancada_B > Renomear e use Reserva_B.\n"
        "2. Confira o nome na lista.\n"
        "3. Para remover uma copia dispensavel, escolha Excluir e confirme.\n"
        "Resultado: Renomear preserva medidas e nao substitui outro nome existente. Excluir remove o registro; segurar cancela antes da confirmacao.",
        "O mesmo armazenamento escolhido ao entrar em Chaves.", WARN_STORAGE
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Comparar",
        "Abre dois registros salvos e apresenta diferencas de comprimento, largura, espessura, posicoes, cabeca e tipo.",
        "Comparar geometria informada, incluindo dimensoes de cada face quando ambos sao cruciformes.",
        "Situacao: Comparar duas referencias medidas por voce.\n"
        "1. Salve A com largura 8,20 mm e B com 8,00 mm, como exemplo didatico.\n"
        "2. Abra Catalogo > Comparar e escolha A, depois B.\n"
        "3. Inverta a ordem para conferir o sinal.\n"
        "Resultado: A menos B = +0,20 mm; invertido = -0,20 mm. Medida ausente aparece nao informada. Igualdade geometrica nao comprova compatibilidade com fechaduras.",
        "Dois registros salvos em /MaliKeys/.",
        "Semelhante indica igualdade de uma categoria informada; nao indica compatibilidade com fechaduras."
    ),
    WIKI_ENTRY(
        MALI_KEYS, "Texto, data e configuracoes",
        "O editor de texto usa o encoder para escolher caracteres, Espaco, Apagar ultimo e Concluir texto.",
        "Editar nome, fabricante, perfil, notas e data sem teclado externo.",
        "Situacao: Nomear uma referencia e datar a medicao.\n"
        "1. Gire ate cada caractere e clique para inserir Bancada_A.\n"
        "2. Use Apagar ultimo para corrigir e Concluir texto para confirmar.\n"
        "3. Data de exemplo: 2026-09-14; tambem pode ficar vazia.\n"
        "Resultado: Segurar cancela o texto em edicao. * indica mudancas pendentes; salve para persisti-las. Passo e guias valem so na sessao, sem alterar as medidas salvas.",
        "Tela e encoder. Data inicial usa o relogio do sistema quando disponivel.",
        "Alteracoes pendentes sao sinalizadas com *. Voltar ao menu Chaves mantem o rascunho; sair solicita confirmar descarte."
    ),

    // Mali Counter
    WIKI_ENTRY(
        MALI_COUNTER, "Counter Suite",
        "Reune monitores de Wi-Fi, BLE, RF, NFC, IR e 2,4 GHz, com painel de ultimo estado e registro de eventos.",
        "Observar atividade dos receptores disponiveis e consultar resultados sem confundir eventos com dispositivos unicos.",
        "Situacao: Observar seu controle IR e sensor BLE.\n"
        "1. No monitor IR, pressione o controle proprio uma vez e depois mantenha pressionado.\n"
        "2. Compare sinais e repeticoes; clique para salvar a amostra.\n"
        "3. No BLE, observe seu sensor por varios ciclos.\n"
        "Resultado: Repeticoes IR nao sao novos controles. Anuncios BLE nao sao dispositivos unicos. Girar muda paginas; no RF, clicar limpa o pico da sessao.",
        "Radios integrados e perifericos conforme a configuracao e disponibilidade.",
        "A ferramenta informa quando o hardware esta indisponivel. Contagens dependem do periodo observado."
    ),
    WIKI_ENTRY(
        MALI_COUNTER, "Testes de resiliencia",
        "O item Testes de resiliencia da categoria Counter abre o laboratorio com modos, intensidade, simulacao e metricas ao vivo.",
        "Comparar ensaios controlados no dispositivo ou na WebUI.",
        "Situacao: Medir respostas do seu roteador por ICMP.\n"
        "1. Conecte o Wi-Fi e consulte o IP real do gateway.\n"
        "2. Em Wi-Fi, escolha latencia ou perda de pacotes, desligue Simulacao e informe esse IPv4.\n"
        "3. Teste por 10 s, pare e salve o resultado.\n"
        "Resultado: Exemplo: 20 sondas concluidas e 1 sem resposta = 5% de perda. Latencia usa respostas recebidas. Sem resposta pode ser bloqueio ICMP, nao queda da Internet.",
        "Os modos disponiveis acompanham as capacidades do hardware.",
        "Simulacao nao transmite. Modos ativos exigem confirmar autorizacao e podem afetar o alvo de laboratorio."
    ),
    WIKI_ENTRY(
        MALI_COUNTER,
        "Painel / Varredura completa",
        "Executa ciclos controlados de varredura Wi-Fi, BLE, nRF24 e RSSI CC1101 e mostra contagens no painel.",
        "Compara atividade observada pelos radios sem tratar eventos como dispositivos unicos.",
        "Situacao: Observar um sensor proprio entrando em atividade.\n"
        "1. Leia o painel com o sensor desligado.\n"
        "2. Ligue-o e aguarde os ciclos dos radios.\n"
        "3. Compare as paginas e os horarios dos eventos.\n"
        "Resultado: Contadores sao observacoes, nao inventario exato. Um unico sensor pode produzir varios eventos; nem todo radio observa simultaneamente.",
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
    emitSection(state, "EXEMPLO PRATICO", entry.example, MaliUI::TEXT_PRIMARY, maxChars);
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
        case Category::WIFI_INSPECTOR: return "WI-FI INSPECTOR";
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
