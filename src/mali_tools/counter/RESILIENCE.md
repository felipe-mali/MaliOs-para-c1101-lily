# COUNTER — Testes de Resiliência

Entrada: **Mali Tools → COUNTER**. O Counter Suite anterior continua disponível.
O mesmo motor atende ao menu do T-Embed e à WebUI existente. Um teste por vez;
o display mostra o estado publicado pelo motor, inclusive quando o início vem da WebUI.

## Arquivos desta etapa

Criados:

- `src/mali_tools/counter/CounterLab.h` e `CounterLab.cpp`: configuração, estados,
  estatísticas, fila, execução cooperativa, menus, desenho e histórico.
- `src/core/wifi/CounterWebApi.h` e `CounterWebApi.cpp`: rotas autenticadas no servidor existente.
- `tests/counter_resilience/web_test.cjs`: integração da WebUI em desktop e celular.
- Este documento e capturas `tests/counter_resilience/{desktop,mobile}.png`.

Adaptados:

- `src/mali_tools/counter/counter_main.h`: métricas e interface limitada de alvos.
- `wifi_counter.cpp`, `ble_counter.cpp`, `rf_counter.cpp`, `ir_counter.cpp`,
  `nfc_counter.cpp` no mesmo diretório: exportação de métricas/alvos e intervalos.
- `src/core/menu_items/MaliToolsMenu.cpp`: entrada COUNTER.
- `src/core/display.cpp`: consumo da fila na tela de menu.
- `src/core/wifi/webInterface.cpp`: registro das rotas e consumo da fila na tela WebUI.
- `embedded_resources/web_interface/index.html`, `index.css`, `index.js`: interface,
  controles, gráfico Canvas e polling. `include/webFiles.h` é gerado pelo build.

## Recursos reais e simulados

| Módulo | Execução real | Somente simulação |
|---|---|---|
| Wi-Fi | Scan passivo de SSID/BSSID, RSSI, canal e segurança; conexão/reconexão do próprio cliente; ICMP para IPv4 explícito; estabilidade de sinal; monitor de canais | Throughput sample: não há adaptador de servidor de laboratório configurado, limitado e cancelável neste fluxo |
| BLE | Scan passivo, endereço/tipo/nome/RSSI/UUID anunciado/connectable; sinal e advertisements; conexão/reconexão do próprio cliente a alvo connectable selecionado | Service enumeration e notification test: ainda não há seleção de serviço/característica e assinatura cancelável integrada ao Counter; nenhum valor arbitrário é escrito |
| RF | CC1101: espectro, atividade, RSSI e eventos em RAM; varredura estreita de 41 pontos ao redor do centro | Modo SIMULATION explícito; Counter não transmite RF |
| IR | Recepção, protocolo, bits, contagem de sinais/repetições, overflow e intervalos entre capturas | Repeat test: a captura do receptor demodulado não determina a portadora; o fluxo existente de transmissão permanece separado |
| NFC | PN532 I2C/SPI: consulta de UID ISO14443A, leituras repetidas, estabilidade e tempos | Modo SIMULATION explícito; nenhuma autenticação, leitura de memória ou escrita de tag |
| System | Jitter de temporizador e heap livre/mínimo | Eventos artificiais para exercitar UI, timers, resultados e STOP |

Todos os módulos oferecem simulação. Nela, o motor não inicia scanner, conexão,
transmissão ou leitura NFC. Os eventos são determinísticos (uma falha a cada onze
tentativas), identificados como SIMULATION na tela, na WebUI e nos resultados.
Alvos simulados não podem ser usados para iniciar conexões reais.

Os monitores e drivers existentes são reutilizados: Arduino WiFi/ESP-IDF,
NimBLE, driver CC1101/helpers de RF, IRrecv, PN532/readUidOnly, input `check`,
`loopOptions`, TFT/tema, guard de LED e SD/LittleFS. ICMP utiliza `esp_ping` do SDK;
não foi adicionado servidor ou framework web.

## Configuração e significado das métricas

- Duração: 5, 10, 30, 60 segundos; personalizada 1–3600; zero = Unlimited até STOP.
- Intervalo personalizado limitado a 60.000 ms. HIGH usa o mínimo do modo,
  MEDIUM usa duas vezes o mínimo, LOW usa quatro vezes o mínimo.
- Mínimos: conexão/reconexão 2.000 ms **entre tentativas**; ICMP 500 ms;
  scan Wi-Fi 4.000 ms; estatísticas BLE 1.000 ms; NFC 250 ms;
  RF/IR/System 100 ms. Nenhum ajuste aumenta potência de transmissão.
- Wi-Fi/BLE: SCAN → SELECT → CONFIGURE → RUN → RESULTS. Até 32 alvos na seleção,
  com validade de dois minutos para conexões reais. BLE exige `connectable`.
- Conexão Wi-Fi usa SSID/senha de rede pessoal ou rede aberta; não configura
  credenciais EAP/Enterprise. A senha não entra em status, histórico ou arquivos;
  o formulário Web a limpa após enviar. RUN AGAIN exige digitá-la novamente.
- O teste real de conexão Wi-Fi exige rádio desligado antes de iniciar. Isso
  impede derrubar a conexão usada pela WebUI. Use o menu local para esse teste,
  ou simulação enquanto a WebUI está conectada. O scan passivo compartilha o rádio.
- ICMP: payload de 32 bytes, timeout de 500 ms, no máximo duas sondas/s, host IPv4
  unicast informado pelo usuário e Wi-Fi STA conectado. SENT conta sondas concluídas;
  uma sonda cancelada pelo STOP não entra como perda. Latência refere-se às respostas.
- RSSI MIN/MAX/AVG usa amostras do dashboard, não cada pacote recebido pelo rádio.
  Em Wi-Fi sem alvo, o valor é a média das redes do scan; em BLE sem alvo, o sinal
  mais forte visível. Contagem de advertisements é acumulada pelo callback existente.
- CHANNEL MONITOR: barras mostram presença de APs por canal, não ocupação do ar.
  A tabela resume até 32 resultados; as barras do scanner podem incluir mais redes.
- RF: centro padrão 433,92 MHz; centro ±0,2 MHz deve estar integralmente em
  300–348, 387–464 ou 779–928 MHz. Antena e módulo determinam sensibilidade real.
- IR: polling configurável do decoder; intervalos altos podem perder sinais.
  REPEATS corresponde a repetições detectadas; FAIL corresponde a overflow;
  TIMING mede intervalo entre capturas, não frequência de portadora.
- NFC: uma consulta sem tag também conta FAIL; isso não indica falha de autenticação.
  Tempos incluem todas as consultas, bem-sucedidas ou não; timeout de UID é 50 ms.
- System: SUCCESS indica heap livre acima de 16 KiB naquele tick; não é um
  diagnóstico completo da integridade da memória.

## STOP e recursos

`BACK` ou clique no encoder solicita STOP; segurar o encoder também para, pois o
input existente repete o evento enquanto pressionado. Os primeiros 350 ms ignoram
SELECT residual do comando RUN; BACK e o STOP da WebUI permanecem ativos.

O endpoint STOP apenas sinaliza a parada. A execução cooperativa verifica a flag
a cada iteração, para novas tentativas, publica STOPPING, cancela o scanner/ping ou
cliente que criou, encerra receptores e publica STOPPED. Não desconecta clientes
externos. Um teste não assume uma pilha BLE já inicializada por outra ferramenta.
O estado prévio de auto-reconnect Wi-Fi é restaurado ao liberar o rádio próprio.

O laço usa `delay(5)` e redraw de regiões a 5 Hz. Conexões BLE são assíncronas,
com timeout de 3 s; Wi-Fi é verificado cooperativamente, com timeout de 10 s.
Chamadas de inicialização/encerramento dos SDKs ainda podem atrasar a confirmação
do STOP. A latência física máxima de parada precisa ser medida no hardware;
não há garantia de tempo real rígido.

## WebUI e armazenamento

No servidor/autenticação já existentes, prefixo `/api/counter/`:

| Método | Rotas |
|---|---|
| GET | `capabilities`, `status`, `targets`, `history` |
| POST | `scan`, `start`, `stop`, `save`, `settings` |

`start`/`scan` recebem `config` JSON como campo de formulário (até 2 KiB).
Há validação de tipos, intervalos, duração, disponibilidade, seleção e autorização.
Os callbacks AsyncTCP não desenham nem executam operações de rádio: enfileiram o
teste para a tela de menu/WebUI. START recusa dispositivo ocupado em outra ferramenta.

O navegador faz polling HTTP JSON a cada segundo, somente enquanto a seção está
visível, sem sobrepor consultas e com timeout de 5 s. STOP usa requisição independente.
O botão permanece fixo no rodapé em celular e sticky no painel em desktop.
Gráficos usam Canvas nativo e o histórico de sinal/tempo tem 60 pontos.

Resultados permanecem em RAM até SAVE RESULT. Resumos JSON vão para
`/MaliTools/Counter/result_NN.json`, no SD já montado ou LittleFS. Sem gravação
contínua durante RUN. O limite padrão é 20, configurável de 1 a 50 nesta sessão;
esse ajuste retorna a 20 após reboot. O próximo SAVE aplica a redução de limite.
Slots são reutilizados; a sequência também reinicia no reboot. Não há log ilimitado.

## Validação e limites

O build alvo é `lilygo-t-embed-cc1101`. A disponibilidade exibida vem do build e da
configuração de pinos/módulos; a inicialização verifica o periférico. Rádio ausente,
pilha ocupada ou memória insuficiente produz ERROR. Wi-Fi é de 2,4 GHz; BLE utiliza
o ESP32-S3; RF exige CC1101; NFC exige PN532 configurado; IR exige receptor configurado.

Teste de navegador: `node tests/counter_resilience/web_test.cjs`, com
`PLAYWRIGHT_MODULE` apontando para Playwright instalado quando necessário.
`COUNTER_EMBEDDED=1` testa os assets minificados e descompactados de `include/webFiles.h`.
O checksum do build confirma a atualização das fontes. As APIs são fixtures em memória: esse teste
não substitui ensaio do firmware e dos periféricos no T-Embed.

Verificação física pendente: STOP durante scan/conexão/consulta PN532, coexistência
BLE/WebUI, reconexão de alvos próprios, histórico em SD/LittleFS e consumo de heap
dinâmico com cada rádio. A compilação informa RAM estática, não pico de heap de SDKs.

Validação automatizada aprovada nesta entrega:

- COUNTER com assets embarcados: desktop e celular, categorias, intensidades
  distintas, simulação, scan/select, autorização e STOP durante polling travado.
- Regressão KEY GAUGE com assets embarcados: mouse/touch, limites, edição,
  carga/gravação/exclusão/duplicação/preview e respostas inválidas.
- `tests/counter/metrics_test.cpp`: static_asserts de métricas, faixas RF e
  validação de UID PN532, compilados com `xtensa-esp32s3-elf-g++ -std=c++17 -fsyntax-only`.
- Saída dos testes web em `tests/counter_resilience/validation.log`.

## Build entregue

`lilygo-t-embed-cc1101`: **SUCCESS**, em 447,59 s.
Log: `counter-resilience-verified-build.log`.
Imagem combinada: `Bruce-lilygo-t-embed-cc1101.bin` (4.519.008 bytes, offset 0x0).
Imagem de aplicação: `.pio/build/lilygo-t-embed-cc1101/firmware.bin`.

| Recurso | Build anterior KEY GAUGE WebUI | Build COUNTER | Diferença |
|---|---:|---:|---:|
| Flash informada pelo PlatformIO | 4.402.022 B | 4.452.914 B | +50.892 B (~49,7 KiB) |
| RAM estática | 150.528 B | 155.792 B | +5.264 B (~5,1 KiB) |

O build usa 26,5% da flash e 47,5% da RAM informadas pelo ambiente. Esses números
não incluem o pico de heap dos scanners, clientes, respostas JSON e tarefas do SDK.
A tabela temporária de seleção ocupa aproximadamente 3,5 KiB quando necessária;
o pico adicional total requer medição no T-Embed. O firmware foi compilado e os
testes automatizados passaram; não foi gravado nem ensaiado fisicamente nesta etapa.
