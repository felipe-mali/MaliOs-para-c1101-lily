# Wi-Fi Inspector — MaliOS

Entrada: **Rede → Wi-Fi → Wi-Fi Inspector**. A ferramenta mantém todos os itens
anteriores do menu Wi-Fi. O acesso da WebUI é pelo navegador do dispositivo;
não foi criado um segundo scanner ou servidor web.

## Primeiro inventário

1. Conecte o MaliOS à sua rede. Abra o Inspector e confira IP, máscara, gateway,
   SSID, BSSID e sub-rede. A rede precisa oferecer IPv4 de cliente, com máscara
   contígua entre /1 e /30.
2. Escolha **Escanear Rede**. A tela mostra a fase, o progresso, a quantidade e
   o endereço atual. Clique ou segure para cancelar.
3. Abra **Dispositivos Encontrados** e selecione um IP. Confira MAC, fabricante,
   nomes recebidos, portas e tipo provável. Compare com o próprio equipamento.
4. Em **Gerenciamento**, marque como conhecido, renomeie e adicione uma nota.
   Exemplo: `Impressora Financeiro`, nota `Patrimonio 123`.
5. Depois de conferir o inventário, **Dispositivos Conhecidos → Criar Baseline
   Atual** permite marcar os MACs encontrados como conhecidos, com confirmação.
6. Use **Atualizar scan**, **Novos Dispositivos** e **Resultado / alterações**
   para comparar as observações da sessão.

Na fonte bitmap do T-Embed, a legenda usa `[V]` conhecido, `[!]` novo MAC e `[?]`
não identificado. Uma porta 631 ou 9100 sugere impressora; 554 sugere câmera;
445 sugere PC/NAS/servidor. Essas são hipóteses, não identificação garantida.
Nomes DNS/mDNS e anúncios são declarações do equipamento. Um serviço sugerido
por uma porta não confirma o software que a atende.

MAC localmente administrado recebe o aviso **MAC privado/aleatório**. Esse bit
não prova aleatoriedade: pode ser um endereço fixo configurado manualmente.
O prefixo não é usado para atribuir fabricante nesses casos. Um novo MAC pode
ser o mesmo telefone; não identifica permanentemente uma pessoa.

## Alcance e limites

- Até **254 IPv4 por faixa**, **64 dispositivos por scan**, **96 MACs no histórico
  por rede/AP** e **oito bases** no armazenamento selecionado. Ao encher o
  histórico, os registros existentes são preservados e o limite é informado.
- Em /24, a faixa padrão cobre os endereços utilizáveis. Em rede maior, começa
  no bloco /24 do T-Embed. **Configurações → Início da faixa** permite escolher
  outro início dentro da sub-rede. O gateway também é consultado.
- ARP: uma solicitação por vez, intervalo de aproximadamente 30 ms e leitura
  frequente da tabela. A tabela **não é limpa**. Portanto, pode conter cache
  recente, e a presença observada não equivale a uma associação verificada.
- TCP: um socket não bloqueante de cada vez; fecha após cada tentativa.
  Portas: 22, 53, 80, 139, 443, 445, 515, 554, 631, 1883, 8008, 8080, 8443, 9100.
  Timeout configurável de 80, 120 ou 200 ms. Não envia comandos de aplicação.
- DNS: tenta PTR reverso apenas se o resolvedor configurado estiver na própria
  sub-rede. Não envia os IPs locais a um resolvedor público.
- mDNS: consulta PTR reverso por UDP 5353, solicitando resposta unicast; também
  aproveita registros A/SRV compatíveis recebidos. Muitos aparelhos não
  respondem PTR reverso. Não é uma enumeração completa de todos os serviços.
- SSDP: uma consulta M-SEARCH e janela de aproximadamente 1,3 s; guarda um
  serviço anunciado por host. Não acessa as URLs `LOCATION` recebidas.
- DNS/mDNS usam janela de 200 ms por consulta; respostas tardias podem faltar.
  Datagramas são limitados a 1.500 bytes, 64 registros e nomes de 255 caracteres
  durante o parsing; campos exibidos/armazenados são menores e sanitizados.
- Limite global de **90 s**. Cancelamento, falta de conexão, mudança de IP/AP ou
  saturação tornam o resultado parcial. Scans parciais não concluem ausências.
  Uma mudança de rede impede mesclar os resultados com a base anterior.
- **Sem resposta não significa saída definitiva.** Repouso, firewall, isolamento
  de clientes e caches afetam resultados. O Inspector não apresenta uma ausência
  como prova de que um cliente foi desconectado do AP.

Os ajustes de scan valem na sessão. Não há task de scan periódico, task por IP,
timer permanente, captura promíscua ou alteração de modo/canal da conexão.
As bibliotecas Arduino WiFi/NetworkUDP, lwIP e a interface STA já existentes são
reutilizadas. O mDNS compartilhado da WebUI não é iniciado nem encerrado pelo
Inspector. Os objetos e sockets próprios são liberados ao sair.

## Histórico

Pasta **`/MaliInspector/`**, no SD montado ao entrar ou, alternativamente, no
LittleFS. A escolha permanece fixa durante a sessão. Nome do arquivo inclui
BSSID, endereço de rede e prefixo; o cabeçalho também confere SSID. Assim, APs
distintos abrem referências distintas, inclusive durante roaming.

Formato binário `MWI1`: cabeçalho de 65 bytes, registros explícitos de 378 bytes
e CRC32 final de quatro bytes, sem depender do padding das estruturas C++.
Persiste MAC, último IP, nomes/notas, hostname, mDNS, anúncio, portas/evidências,
primeira/última data conhecida, contagem de observações e marca de conhecido.
Fabricante é reconstituído pela base OUI local. Flags de presença/novidade são
da sessão e não pretendem representar associação atual após reiniciar.

Cada MAC conta no máximo uma vez por scan, mesmo com várias respostas. DHCP
pode mudar o IP sem perder o nome e a marca de conhecido. Observações sem MAC
ficam apenas no resultado da sessão. Datas ausentes são exibidas como **Sem hora
sincronizada**, sem inventar um horário. Sincronizar depois não recupera a data
original desconhecida.

Gravação ocorre ao concluir/encerrar uma varredura com observações ou ao editar
metadados, nunca a cada pacote. Usa `.tmp`, verificação de leitura/CRC e `.bak`
na substituição. Falha mantém a indicação de alteração pendente. Arquivo inválido
não é sobrescrito automaticamente. **Limpar histórico desta rede** exige
confirmação; preserve uma cópia antes se precisar investigar o arquivo.

## Gerenciamento administrativo

**Esta versão inclui apenas o adaptador Genérico, sem API de desconexão.**
O anexo permite uma implementação inicial com estrutura genérica; não foi
presumido modelo, firmware ou endpoint de um roteador não informado.

Em **Configurações → Gerenciamento do Roteador**, é possível consultar o tipo,
ajustar um IP da sub-rede durante a sessão e testar o suporte administrativo.
O resultado informa que não existe adaptador compatível nesta versão. Nenhum
usuário ou senha é solicitado, armazenado ou enviado. **Apagar credenciais**
encerra a sessão administrativa; no adaptador atual não há credenciais salvas.

**Dispositivo → Gerenciamento → Desconectar da rede** mostra MAC e IP antes da
confirmação. No Genérico, retorna a mensagem de falta de suporte e não envia
comando. Não implementa blacklist, jamming, deauth spoofada ou fallback de ataque.
O Inspector nunca chama as funções antigas de ataque do menu Wi-Fi.

`RouterManager` define suporte, autenticação, lista de clientes, desconexão
individual e logout. Distingue solicitação aceita de desconexão verificada.
Um futuro adaptador precisa de API documentada, TLS validado, autenticação,
tratamento seguro de segredos, timeouts/cancelamento e validação no roteador.
Não existem adaptadores OpenWrt, MikroTik ou UniFi disfarçados de Genérico.

## Base OUI

58 prefixos globais, abrangendo os 20 fabricantes solicitados. É uma amostra
pequena e extensível, não uma base completa de cada fabricante. Endereço não
reconhecido retorna **Desconhecido**. Não há consulta à Internet no aparelho.

Fonte: [exportação de fabricantes do Wireshark](https://www.wireshark.org/download/automated/data/manuf),
consultada em 15/09/2026. Os prefixos e nomes de registro usados estão em
`tests/wifi_inspector/oui_source.json`. A consulta direta ao registro IEEE
retornou HTTP 418; não foram reutilizados os prefixos demonstrativos/inconsistentes
da tabela de outra ferramenta do firmware.

## Integração e validação

A análise inicial identificou `WifiMenu.cpp`, `MaliUI`, `MaliInput`, o catálogo
PT-BR, `ScrollableTextArea`, o teclado, SD/LittleFS, Arduino WiFi/NetworkUDP,
`ARPScanner`, `HostInfo` e `getManufacturer`. O scanner antigo combina UI e
ações próprias e limpa ARP; o identificador antigo consulta uma API pública.
O Inspector reutiliza as bibliotecas e componentes comuns, com um fluxo
incremental separado para preservar essas ferramentas.

`device_model`, `device_scanner`, `discovery_protocols`, `device_database`,
`device_classifier`, `oui_database`, `router_manager` e `wifi_inspector` separam
responsabilidades. Textos principais ficam em `InspectorPtBr.h`; os demais são
PT-BR com grafia ASCII compatível com a TFT. A ajuda foi adicionada ao firmware,
WebUI e terminal. Não há alteração de board, partição, pinagem ou driver.

Os testes em `tests/wifi_inspector/` executam o modelo, classificador, parsing e
persistência reais com um armazenamento em memória: mudança de IP, cancelamento
parcial, faixas, limite cheio, corrupção, gravação/rename com falha e recuperação.
Pacotes truncados, compressão DNS inválida e 20.000 datagramas pseudoaleatórios
exercitam o parser. O adaptador em memória não substitui teste físico de SD.

Compilação: `pio run -e lilygo-t-embed-cc1101`. O resultado medido da entrega fica
em `ENTREGA.md` após a validação. Boot, Wi-Fi real, encoder, latência de cancelamento,
heap/PSRAM em execução e ausência de watchdog exigem teste no aparelho; não são
comprovados apenas por compilar ou pelos testes de computador.
