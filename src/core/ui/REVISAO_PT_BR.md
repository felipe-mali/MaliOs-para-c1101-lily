# Revisão de interface e navegação

## Funcionalidades e acessos

A revisão compara os 18 menus registrados com o código anterior ao Gear
(`e7265ac7`) e com o início desta revisão (`34816e7f`). O menu Mali Counter
estava registrado, mas não era incluído nas novas categorias: seu acesso foi
restaurado em COUNTER, junto dos Testes de Resiliência. Conexões passou a
integrar o mesmo registro e respeitar a configuração de menus ocultos.

`MenuRoute.h` concentra o vínculo entre identificadores estáveis e categorias.
Um novo menu registrado recebe Ferramentas como caminho padrão, evitando que
uma nova implementação fique sem acesso por falta de um filtro de nomes.

| Categoria | Entradas preservadas |
| --- | --- |
| Rede | Wi-Fi, Bluetooth/BLE, Ethernet, Conexões |
| Rádio | RF/CC1101, nRF24, LoRa, FM, IR, NFC/RFID, GPS |
| Ferramentas | Mali Tools, utilitários, aplicativos, BadUSB/HID, USB, scripts/JavaScript |
| Counter | Testes de Resiliência e Mali Counter/Counter Suite |
| Arquivos | SD, LittleFS, WebUI e operações de arquivos |
| Sistema | Configurações, relógio e temporizador |

As condições de compilação e disponibilidade de hardware foram mantidas.
Aplicativos ocultados pelo usuário continuam configuráveis. D20, Pixel Paint,
QR, KEY GAUGE, rede, serviços de internet e compartilhamento permanecem nos
respectivos menus. Nenhum driver, biblioteca ou protocolo RF foi modificado.

## Componentes visuais

- Os menus de categorias e ações em `src/core/menu_items` usam o Gear comum.
  A suíte BLE também usa esse componente para seus menus e submenus.
- Seletores de valores, arquivos, resultados e históricos continuam listas,
  com seleção arredondada. Não se altera a representação de pixels, espectros,
  imagens, QR ou gráficos apenas para arredondar sua geometria.
- Cabeçalhos, rodapés, avisos e resultados usam os componentes Mali. Caixas de
  teclado, relógio e telas próprias usam `drawRoundedBox`/`drawRoundedFill`.
- D20 atualiza os cartões durante a rolagem, sem limpar a tela a cada quadro.
  Pixel Paint distribui tela e painel conforme a orientação; os pixels e seu
  formato salvo continuam iguais.
- KEY GAUGE adapta informações e lista de pontos à largura disponível.
  Espessura, largura, níveis e formato `.mkg` não mudaram.
- Rótulos longos do Gear usam duas linhas; cabeçalhos e descrições respeitam
  o espaço disponível. A quebra de textos passou a preservar caracteres em
  palavras longas e a respeitar quebras de linha explícitas.
- Relatórios BLE passam a permitir rolagem, em vez de perder as linhas que
  ficavam fora da tela.

O buffer do Gear permanece limitado a 24 linhas RGB565 (até 15.360 bytes a
320 pixels), liberado antes de abrir uma ferramenta. Não há nova animação em
repouso, framebuffer de tela inteira, biblioteca gráfica ou servidor Web.

## PT-BR e contratos externos

Não havia infraestrutura de idiomas; a maior parte da interface já possuía
português diretamente no código. Foram reunidos 541 textos no catálogo
`pt_br.tsv`, que gera constantes em `PtBr.h`. Somente constantes usadas são
referenciadas pelo firmware: não há busca em dicionário ou tradução de dados
recebidos durante o desenho. A grafia ASCII da TFT é compatível com sua fonte
bitmap fixa; a WebUI usa UTF-8.

`web_pt_br.tsv` registra a migração dos textos Web. Os estados e nomes de
métricas recebem rótulos portugueses na apresentação; IDs como `RUNNING`,
`STOPPED`, `SUCCESS`, métodos HTTP e nomes de parâmetros permanecem estáveis.
Arquivos do usuário, SSIDs, MACs, payloads, comandos DuckyScript, conteúdo de
scripts e diagnósticos brutos não são traduzidos automaticamente.

A busca de referências Bruce foi revisada. Permanecem caminhos usados pelos
formatos existentes (`bruce.conf`, `/BruceJS`, `/BruceRFID`), valores de rede
editáveis (`BruceNet`, `BruceShell`, domínio/grupo do Responder), nomes de
componentes como Brucegotchi, créditos e autoria. Esses valores identificam
arquivos, serviços ou componentes reais; não são títulos herdados do menu.

Para regenerar somente as constantes:

```powershell
python tests/mali_ui/migrate_locale.py
```

A opção `--apply` é uma ferramenta de migração, não uma etapa obrigatória do
build. Novos textos devem ser revisados antes de entrar no catálogo.

## Validação e limites

- `review_audit.py`: registros antigos preservados, todas as instâncias de
  menu com caminho, callbacks preservados, nenhuma implementação removida,
  rotas HTTP, estados internos, formato dos perfis e drivers preservados.
- `review_test.cpp` e `motion_test.cpp`: verificações em compilação de rotas,
  nomes futuros, quebra de texto, wrap, giro rápido, reversão, animação
  interrompível, clique e pressão longa.
- Playwright em desktop e celular: navegação, layout, login, edição e
  persistência dos perfis, seleção de alvos, limites, autorização e parada do
  Counter durante consulta pendente. Testados fontes e assets gzip incorporados.
- `preview.py`: revisão geométrica com a fonte bitmap do repositório nas duas
  orientações. As imagens são simulações, não capturas do dispositivo.

Os logs desta revisão ficam em `tests/mali_ui/review-*.log`; o log de compilação
final é `mali-ui-review-verified-build.log` na raiz.

### Compilação final

Em 11/09/2026, `pio run -e lilygo-t-embed-cc1101` terminou com **SUCCESS**,
código de saída 0, em 647,71 segundos. O binário combinado foi gerado em
`Bruce-lilygo-t-embed-cc1101.bin` (4.524.752 bytes, offset inicial `0x0`).
O nome do arquivo segue a configuração existente do build.

| Recurso | Referência anterior | Revisão final | Diferença |
| --- | ---: | ---: | ---: |
| RAM estática | 156.216 bytes | 156.208 bytes (47,7%) | -8 bytes |
| Flash da aplicação | 4.459.722 bytes | 4.458.670 bytes (26,6%) | -1.052 bytes |

A referência é o log `mali-rotary-verified-build.log`. Essas medidas são do
linker; não representam pico de heap, PSRAM ou CPU durante execução.

SHA-256 do binário:
`e74481e93ee8823d690aab59aa4a365d018ed9809674dc2ba5b989394168dd54`.

Os avisos foram comparados com `mali-rotary-verified-build.log`. Persistem
avisos anteriores de tipos divergentes no LTO/ODR das bibliotecas de display
e BLE, possível escrita fora dos limites no decodificador AAC de ESP8266Audio,
APIs obsoletas e variáveis não utilizadas. Eles não foram ocultados;
revisar essas dependências exige uma etapa própria e testes no hardware,
sem misturar mudanças de drivers com esta revisão visual.

A validação automatizada não substitui percorrer cada ferramenta no T-Embed.
Não houve gravação nem teste físico nesta etapa: fluidez real, flicker,
polaridade do encoder e consumo máximo de heap sob uso dos rádios ainda
dependem do aparelho. A tradução foi auditada nas fontes da interface; dados
externos e scripts podem apresentar outros idiomas por definição.
