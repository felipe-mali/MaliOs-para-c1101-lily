# Chaves — MaliOS

Acesso: **Ferramentas → Mali Tools → Chaves**. A ajuda fica em **? Ajuda**,
na seção Ferramentas da WebUI e no comando serial `help`.

## Uso

1. Escolha **Chave Plana** ou **Chave Cruciforme** para iniciar um registro.
2. Em **Medir Chave**, informe comprimento total, útil, largura e espessura.
   As medidas são manuais, em mm. Zero significa **não informado**.
3. Em **Editar dados**, informe fabricante, tipo de perfil, cabeça, orientação,
   observações e data. A plana permite lado, canaletas e posições aproximadas.
   A cruciforme possui quatro faces com dados e notas independentes.
4. Em **Visualizar**, gire para alternar faces da cruciforme. Na vista frontal,
   A está acima, B à direita, C abaixo e D à esquerda. O comprimento de cada
   braço é medido do centro da cruz até a borda; sua largura é transversal.
5. **Salvar** grava o registro; **Salvar como** cria uma cópia com outro nome.
   **Catálogo** oferece carregar/visualizar, renomear, excluir e comparar.
   A comparação mostra **A − B**; grandezas ausentes não viram zero no resultado.

Girar altera a opção ou o valor; clicar seleciona/confirma; segurar por
650 ms cancela/volta. O editor de texto oferece caracteres, Espaço, Apagar
último e Concluir texto. Soltar depois de uma pressão longa não gera clique.
Alterações aparecem com `*`; voltar ao menu Chaves mantém o rascunho. Sair
da seção ou iniciar/carregar outro registro pede confirmação de descarte.

Configurações oferece passo de 0,01, 0,10 ou 1,00 mm e guias visuais. Esses
ajustes valem durante a sessão. Data usa `AAAA-MM-DD`, pode ficar vazia e
recebe inicialmente a data do sistema quando o relógio estiver disponível.

## Modelo e representação

- `KeyProfile.h`: medidas inteiras em centésimos de mm, dados da plana e
  quatro faces da cruciforme. Não contém profundidades de corte.
- `KeyComparison.h`: contagem comparável de posições; retorna desconhecido
  quando faltar uma contagem necessária.
- `KeyRenderer.cpp`: cabeça, ombro, haste, ponta, canaletas, hachura da região
  serrilhada e vista frontal em cruz. Marcações de posições ficam fora do corpo.
- `KeyMeasurement.cpp`: entrada manual, texto e leitura de informações.
- `KeyStorage.cpp`/`KeyCodec.h`: operações do catálogo e formato persistente.
- `MaliKeys.cpp`: navegação e estado da sessão.
- `src/core/ui/MaliInput.h`: entrada comum também usada pela ajuda.
- `src/core/ui/KeysPtBr.h`: textos PT-BR constantes, seguindo a arquitetura
  central de traduções, com grafia ASCII adequada à fonte bitmap da TFT.

O desenho ajusta-se ao espaço da tela, não é uma régua física 1:1. Medidas
não informadas usam proporções ilustrativas apenas no desenho, sem preencher
campos ou gravar valores presumidos. A largura não medida da cabeça segue
o formato ilustrativo escolhido. Espessura real, largura real e espaçamento
visual são campos separados; o espaçamento relativo vai de 50 a 150%.
Na cruciforme, a orientação de cada face é relativa à orientação geral;
o visor mostra a direção resultante, aplicando ambas.
Fabricante é texto informado pelo usuário. Não há identificação comercial,
decodificação de cortes, associação automática a fechaduras ou exportação
para fabricação.

Os limites técnicos são: total/útil/face até 300 mm, largura geral até 100 mm,
espessura e dimensões dos braços até 50 mm, posições de 0 a 20 e canaletas de
0 a 8. São limites de entrada, não especificações de modelos comerciais.

## Armazenamento

O módulo fixa um armazenamento ao entrar: SD montado, ou LittleFS como
alternativa. Essa escolha não muda no meio da edição. A pasta é `/MaliKeys/`,
com até 256 registros `.mkey` de **662 bytes**. Nomes têm até 31 caracteres:
letras ASCII, números, espaços internos, hífen e sublinhado. Renomear não
sobrescreve outro registro, inclusive em cartões FAT sem distinção de caixa.

Formato `MLK1`, versão 1: cabeçalho, tipo, textos de tamanho fixo, medidas
little-endian, configuração da plana, quatro faces e CRC32. A gravação usa
arquivo `.tmp`, leitura de verificação e `.bak` durante a substituição. Na
próxima abertura, um backup sem destino é restaurado. Arquivos temporários
incompletos não são listados. O nome do arquivo é a identidade autoritativa,
inclusive após renomear pelo gerenciador existente.

KEY GAUGE e seus arquivos `/MaliTools/KeyGauge/*.mkg` continuam disponíveis.
Não há conversão automática: níveis relativos antigos não representam medidas
físicas. A nova seção é controlada pela tela ou pelo navegador do dispositivo;
a WebUI também oferece ajuda e acesso aos arquivos, sem um segundo editor
de Chaves com estado independente.

## Verificação

`tests/mali_keys/profile_test.cpp` contém testes avaliados pelo compilador
para ida e volta de ambos os tipos no formato salvo, quatro faces, corrupção,
enum inválido mesmo com CRC correto, dimensões, textos, datas, contagens,
giro acumulado e pressão longa sem clique extra. `tests/mali_ui/review_audit.py`
confere os 18 menus, callbacks, rotas, estados de API e o formato antigo.

Os testes Playwright verificam ajuda expandida e navegação em 1440 e 390 px,
além dos fluxos anteriores de KEY GAUGE e Counter. As prévias em
`tests/mali_keys/` usam dados fictícios e a fonte bitmap do repositório; são
simulações geométricas, não capturas de hardware.

O módulo redesenha os cartões somente quando há alteração, sem animação em
repouso ou framebuffer adicional. Medidas do linker e resultado final ficam
em `mali-keys-final-build.log` e no relatório de entrega nesta pasta.

Não houve gravação no T-Embed nesta etapa. Validar no aparelho: medidas
extremas, resposta do encoder, remoção de SD, falha de alimentação durante
gravação, pico de heap e legibilidade sob diferentes configurações de fonte.
