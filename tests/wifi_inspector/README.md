# Testes do Wi-Fi Inspector

Os testes C++ executam os arquivos reais do módulo. `stubs/FS.h` substitui
apenas o armazenamento Arduino por um sistema em memória, permitindo injetar
falhas sem acessar cartões ou dispositivos da rede.

Foi usado Zig 0.13.0 como compilador C++ portátil no Windows. A ferramenta fica
na pasta temporária ignorada pelo Git, sem mudar o compilador do firmware:

```powershell
python -m pip install --target .codex_tmp/host-tools ziglang==0.13.0
./tests/wifi_inspector/run_host.ps1
python tests/wifi_inspector/review.py
python tests/mali_ui/review_audit.py
```

O teste cobre identidade por MAC após DHCP, retenção de nomes, novidade e limite
de histórico, scan parcial, faixas, ida e volta do arquivo, CRC/corrupção,
falha de escrita/commit e recuperação de backup. Exercita PTR válido, compressão
DNS, pacotes truncados, 20 mil datagramas pseudoaleatórios e classificação.

`review.py` complementa os testes com preservação dos callbacks do menu, limites
e recursos proibidos no scanner, cobertura OUI e ajuda. As verificações de texto
não substituem testes de execução do scanner real em rede.

WebUI embutida:

```powershell
python tests/mali_keys/verify_release.py
$env:MALI_EMBEDDED='1'
node tests/mali_ui/web_test.cjs
```

Playwright precisa estar disponível; `PLAYWRIGHT_MODULE` permite indicar a
instalação existente. O teste abre todos os tópicos da ajuda em 1440 e 390 px.

O firmware completo usa o ambiente original:

```powershell
pio run -e lilygo-t-embed-cc1101
```

Boot, conexão Wi-Fi, resposta do encoder, cancelamento físico, consumo de heap,
PSRAM e watchdog precisam ser conferidos no T-Embed. Nenhum teste desta pasta
grava firmware ou envia ataques/solicitações administrativas a um roteador.
