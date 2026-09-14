# Entrega — Chaves, ajuda e PT-BR

## Compilação final

**SUCCESS** em 14/09/2026 para `lilygo-t-embed-cc1101`, saída 0, duração
835,24 segundos. Log: `mali-keys-final-build.log`, na raiz do projeto.

| Recurso | Revisão anterior | Entrega final | Diferença |
| --- | ---: | ---: | ---: |
| RAM estática | 156.208 bytes | 156.216 bytes (47,7%) | +8 bytes |
| Flash da aplicação | 4.458.670 bytes | 4.510.354 bytes (26,9%) | +51.684 bytes |

Referência: `mali-ui-review-verified-build.log`. Essas são medidas do linker,
não picos de heap/PSRAM durante execução. Não foram introduzidas novas
assinaturas de warnings em relação aos logs anteriores de builds bem-sucedidos.

Binário combinado: `Bruce-lilygo-t-embed-cc1101.bin`, **4.576.448 bytes**,
offset inicial `0x0`. O nome do arquivo segue o processo de build existente.

SHA-256:
`9dcb76ad9994c9571a34d7ff216e3f612fb745333b0f14e23405258863fdf627`.

## Alterações

- Nova seção **Chaves**, com chave plana, cruciforme, medidas manuais em mm,
  metadados, notas, quatro faces, catálogo e comparação de dois registros.
- Catálogo independente em `/MaliKeys/`, sem alterar os perfis `.mkg` ou
  remover o KEY GAUGE anterior. Salvar, carregar, renomear e excluir mantêm
  nomes validados e proteção contra substituição involuntária.
- Desenho lateral com cabeça, ombro, haste, ponta e canaletas; região
  serrilhada ilustrativa; cruz frontal com face ativa. Não são calculadas
  profundidades de corte ou identificações comerciais.
- Entrada comum com giro acumulado, confirmação ao soltar um clique e
  retorno ao segurar. A ajuda também passou a aceitar pressão longa.
- **142 páginas de ajuda** no firmware: sete de Chaves, nove em Ferramentas
  Mali e três em Mali Counter. Incluídas orientações para KEY GAUGE, Counter,
  D20, Pixel Paint, navegação e armazenamento. Ajuda Web e serial atualizadas.
- **589 entradas no catálogo PT-BR geral**, além do catálogo do módulo.
  Revisados textos indiretos do editor Web, mensagens de armazenamento,
  controles de mídia, clicador USB, RF, estados NFC e rodapés Counter.
  Estados de API, comandos, nomes de teclas, arquivos e dados externos
  mantêm sua identidade. Textos gerados por scripts e dados de dispositivos
  não passam por tradução automática.

## Validação concluída

- Testes C++ avaliados pelo compilador: serialização e leitura de chave plana
  e cruciforme, quatro faces, corrupção, enums, limites, datas e strings.
- Testes de giro acumulado e pressão longa: limitar valores sem transbordar,
  cancelar sem emitir clique ao soltar e confirmar um clique posterior.
- Auditoria de preservação: **18 menus registrados**, callbacks anteriores,
  rotas HTTP, estados internos, formato `.mkg` e implementações preservados.
- Playwright em desktop e celular: navegação, ajuda expandida sem ultrapassar
  a tela, login, edição e persistência do KEY GAUGE, seleção, autorização,
  métricas e parada do Counter durante consulta pendente.
- Testados os fontes Web e os arquivos minificados/gzip incorporados.
  `verify_release.py` confere checksum, textos Web, cobertura de ajuda e
  presença simultânea de Chaves e KEY GAUGE.
- Revisão visual por simulação com a fonte TFT, em 320×170 e 170×320, para
  ambas as chaves e os destaques das quatro faces. Sem captura do aparelho.

Logs e imagens ficam em `tests/mali_keys/`. O manual do módulo é
[README.md](README.md).

## Limites e avisos

Não houve gravação do firmware ou teste físico. A integração real com
SD/LittleFS, interrupção de energia, resposta do encoder, desempenho e pico
de heap devem ser conferidos no T-Embed. Os testes de persistência executados
verificam o formato e sua validação; não simulam falhas elétricas de um cartão.

Os avisos de compilação foram comparados com as compilações anteriores.
Há avisos preexistentes de ODR/LTO no display/BLE, APIs obsoletas, trechos
RFID e possíveis acessos fora de limites em decodificadores de áudio. Não
foram ocultados nem corrigidos por mudanças de drivers nesta tarefa.
