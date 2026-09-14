# MaliOS Gear UI

> A seção [Chaves](../../mali_tools/keys/README.md) acrescenta catálogo de
> referências, medição manual, quatro faces e comparação. A ajuda do firmware,
> da WebUI e do terminal foi atualizada junto com o módulo.

> A revisão atual de navegação, PT-BR, componentes e validação está em
> [REVISAO_PT_BR.md](REVISAO_PT_BR.md). Este documento registra a implementação
> inicial do Gear; nomes de categorias e medidas de build abaixo são históricos.

A interface principal usa seis categorias circulares: NETWORK, RADIO, TOOLS,
COUNTER, FILES e SYSTEM. O item selecionado recebe cartão e ícone maiores;
os vizinhos permanecem parcialmente visíveis. Listas de arquivos, resultados
e opções continuam usando linhas compactas.

## Componentes e arquivos

- `MaliTheme.h`: paleta RGB565, três raios e papéis tipográficos.
- `MaliMotion.h`: seleção circular, transição interrompível e estado do botão.
- `MaliUI.h/.cpp`: ícones geométricos, cabeçalho com indicadores, rodapé,
  cartões, linhas de menu, diálogo, aviso, progresso, seletor, abas e boot.
- `src/core/display.cpp/.h`: integração dos componentes e do modo Gear no
  fluxo existente de opções, incluindo atalhos e navegação remota.
- `src/core/main_menu.cpp/.h` e `menu_items/MaliToolsMenu.cpp`: categorias
  principais e acesso às ferramentas existentes.
- `include/globals.h` e `boards/lilygo-t-embed-cc1101/interface.cpp`: consumo
  atômico dos passos acumulados do encoder, preservando pinos e decodificação.
- `src/core/theme.h`, `src/core/config.cpp` e `src/main.cpp`: tema padrão,
  migração apenas da paleta padrão antiga e sequência de boot.
- `src/mali_tools/counter/CounterLab.cpp/.h`: entrada Gear, métricas ao vivo
  e consulta de execução pendente para liberar o buffer de desenho.
- `src/mali_tools/key_gauge/KeyGauge.cpp`: integração visual do cabeçalho.
- `src/core/wifi/webInterface.cpp`: tela física de conexão da WebUI.
- `embedded_resources/web_interface/index.html`, `index.css`, `index.js` e
  `login.html`: navegação lateral, layout móvel, atalhos e componentes Web.
- `src/modules/mali/MaliWiki.cpp`: atualização de referência visual à WebUI.

## Interação e memória

A seleção muda imediatamente. A transição dura 120 ms; um novo movimento
interrompe a transição anterior e usa 90 ms, sem fila de animações. Não há
animação contínua em repouso. Passos acumulados são reduzidos pelo número de
itens, preservando a posição final e evitando laços proporcionais ao atraso.
O clique é confirmado ao soltar; segurar por 650 ms volta uma vez. Um botão
já pressionado ao entrar só é armado depois de solto.

O Gear usa uma faixa RGB565 de 24 linhas: até 15.360 bytes para largura de
320 pixels, sem framebuffer de tela inteira. O buffer é liberado antes de
abrir aplicativos, menus aninhados ou iniciar uma execução Counter pendente.
Falha de alocação e navegação remota usam desenho estático compacto.

COUNTER mantém os motores, limites e APIs existentes. KEY GAUGE mantém
pontos, espessura, largura e persistência. A WebUI continua em HTML/CSS/JS,
com o servidor existente, sem framework ou servidor adicional.

## Migração e compatibilidade

Foram migrados o menu principal, Mali Tools, entrada e painel Counter,
listas genéricas, avisos, diálogos, progresso, boot padrão, cabeçalho do
Key Gauge, tela de conexão e estrutura/navegação/login da WebUI.

Corpos de telas especializadas que desenham diretamente no TFT ainda podem
ter composição própria. A migração não reescreve cada aplicativo legado.
Arquivos de boot personalizados continuam respeitados. Referências Bruce
em autoria/licenças e identificadores compatíveis, como `/bruce.conf`,
`/BruceWebUI/`, `BRUCESESSION` e `bruce.local`, permanecem intencionais.
Credenciais e nomes de rede existentes não são alterados pela reforma visual.

## Validação

- `tests/mali_ui/motion_test.cpp`: verificações em compilação de wrap,
  grandes lotes, reversão, término das transições, overflow do relógio,
  clique e pressão longa.
- `tests/mali_ui/web_test.cjs`: Playwright em 1440 e 390 pixels, navegação,
  layout, atalhos, movimento reduzido e login; suporta os assets gzip
  efetivamente incorporados ao firmware com `MALI_EMBEDDED=1`.
- `tests/key_gauge/web_test.cjs` e `tests/counter_resilience/web_test.cjs`:
  regressões de edição/persistência e parada durante consulta pendente.
- `tests/mali_ui/preview.py`: revisão geométrica de 320×170 e 170×320 com a
  fonte do firmware. As imagens geradas são simulações, não capturas do TFT.

Resultado desta entrega: verificações de movimento aprovadas; suites WebUI,
KEY GAUGE e COUNTER aprovadas em desktop e mobile com os assets incorporados.
O checksum das fontes Web corresponde ao registrado pelo gerador do header.
Logs de navegador: `tests/mali_ui/web-validation.log`,
`counter-regression.log` e `key-gauge-regression.log` no mesmo diretório.

A compilação `lilygo-t-embed-cc1101` terminou com **SUCCESS**, código de saída
0, em 710,02 segundos. O log de entrega fica em
`mali-rotary-verified-build.log`, na raiz.

| Recurso | Build anterior | Build Gear UI | Diferença |
| --- | ---: | ---: | ---: |
| RAM estática | 155.792 B | 156.216 B (47,7%) | +424 B |
| Flash | 4.452.914 B | 4.459.722 B (26,6%) | +6.808 B |

O buffer dinâmico de desenho descrito acima não está incluído na RAM estática.
O binário combinado é `Bruce-lilygo-t-embed-cc1101.bin` na raiz, com
4.525.808 bytes, preparado para offset `0x0`. O nome de saída do processo de
build existente foi preservado. Nenhum dispositivo foi gravado nesta etapa.
O build mantém avisos de bibliotecas legadas (incluindo NimBLE/PN532BLE e
ESP8266Audio), registrados no log; não é um build livre de warnings.

Não houve teste em hardware nesta etapa. Polaridade física do encoder,
latência e cintilação no TFT, consumo de heap em execução e parada por botão
precisam ser confirmados no dispositivo. Os testes de lógica e navegador não
substituem essa verificação.
