#pragma once
namespace MaliText {
inline constexpr char SerialHelp[] = R"HELP(
Comandos internos do terminal. Mantenha os nomes dos comandos como abaixo.

Wi-Fi:
  wifi off - Desconecta o Wi-Fi.
  wifi on - Conecta a uma rede conhecida; sem rede conhecida, inicia modo AP.
  wifi add "SSID" "Password" - Adiciona a rede e a senha informadas.
  arp - Procura hosts por ARP.
  listen - Escuta TCP na porta padrao.
  sniffer - Inicia captura de pacotes brutos.
  webui - Inicia o servidor da interface web.

IR:
  ir rx <timeout> - Le um sinal IR e exibe os dados na serial.
  ir rx raw <timeout> - Le um sinal IR em formato RAW.
  ir tx <protocol> <address> <decoded_value> - Envia um sinal IR decodificado.
  ir tx_from_file <ir file path> [hide default UI true/false] - Envia o sinal de um arquivo; permite ocultar a interface.

RF:
  subghz rx <timeout> - Le RF e exibe os dados na serial. Alias: rf rx.
  subghz rx raw <timeout> - Le RF bruto. Alias: rf rx raw.
  subghz tx <decoded_value> <frequency> <te> <count> - Envia RF decodificado. Alias: rf tx.
  subghz tx_from_file <sub file path> [hide default UI true/false] - Envia RF de um arquivo; permite ocultar a interface.

Audio:
  music_player <audio file path> - Reproduz um arquivo de audio.
  tone <frequency> <duration> - Reproduz um tom de onda quadrada.
  say <text> - Converte texto em fala; exige alto-falante.

Interface e energia:
  led <r/g/b> <0-255> - Altera a cor principal.
  clock - Abre o relogio.
  power <off/reboot/sleep> - Desliga, reinicia ou coloca em repouso.
  nav <next/prev/esc/up/down/select> - Navega pelas opcoes.
  options - Lista as opcoes do menu atual.
  loader list - Lista os identificadores dos aplicativos.
  loader open appname - Abre o aplicativo informado.

GPIO, I2C e arquivos:
  gpio mode <pin number> <0/1> - Configura o pino: 0=entrada, 1=saida.
  gpio set <pin number> <0/1> - Controla o pino: 0=desligado, 1=ligado.
  i2c scan - Procura modulos no barramento I2C.
  storage <list/remove/mkdir/rename/read/write/copy/md5/crc32> <file path> - Gerencia arquivos.
  ls - Equivale a storage list.

Configuracoes:
  settings - Exibe todas as configuracoes.
  settings <name> - Consulta uma configuracao.
  settings <name> <new value> - Altera uma configuracao.
  factory_reset - Restaura a configuracao padrao.

Novas ferramentas, pela tela ou pelo navegador do dispositivo:
  Wi-Fi > Wi-Fi Inspector: scan local, conhecidos, baseline e historico /MaliInspector/.
  Inspector > ? Ajuda: descoberta, MAC privado, limites e exemplos de inventario.
  Gerenciamento generico: sem API administrativa de desconexao nesta versao.
  Ferramentas > Mali Tools > Chaves: plana, cruciforme, medicao em mm e catalogo /MaliKeys/.
  Chaves > Catalogo > Comparar: abre dois registros; diferencas usam A menos B.
  Chaves > ? Ajuda: medidas, quatro faces, texto, salvar, carregar, renomear e excluir.
  KEY GAUGE: editor anterior preservado, com arquivos .mkg separados do catalogo .mkey.
  Counter: testes de resiliencia, simulacao, metricas e parada pela tela ou WebUI.
  D20 e Pixel Paint: dados e desenho local em Ferramentas.
  Encoder: girar escolhe, clicar confirma, segurar volta.

Exemplos praticos (envie um comando por vez):
  1. Identificar o aparelho antes de relatar um erro:
     info
     free
     uptime
     Resultado: dados do dispositivo, memoria disponivel e tempo ligado.
     Anote tambem os passos que provocaram o problema; heap varia durante uso.

  2. Encontrar arquivos salvos:
     storage list /
     storage list /MaliKeys
     Resultado: nomes e tamanhos no armazenamento selecionado pelo firmware.
     Se /MaliKeys nao existir, confira se o registro foi salvo no SD ou LittleFS.
     .mkey e binario: consulte as medidas pelo Catalogo de Chaves.

  3. Consultar um perfil geometrico que voce ja salvou como Bancada_A:
     storage read /MaliTools/KeyGauge/Bancada_A.mkg
     Resultado: texto MKG1 com pontos, niveis, thickness e width.
     thickness=5 e width=90 sao escalas visuais, nao milimetros.
     Troque Bancada_A pelo nome real do arquivo; leitura nao altera o perfil.

  4. Abrir a WebUI:
     wifi on
     webui
     Conecte o computador a mesma rede e abra o endereco mostrado na tela.
     Se bruce.local nao abrir, use o IP do MaliOS em Info do AP.
     wifi on pode iniciar um AP quando nao houver rede conhecida.

  5. Conferir uma opcao antes de navegar remotamente:
     options
     nav next
     options
     Resultado: consulta o menu, avanca uma opcao e permite conferir novamente.
     nav select executa a opcao atual; confira a selecao antes de envia-lo.

Exemplo em Chaves, pela tela ou navegador do dispositivo:
  Salve A com largura 8,20 mm e B com 8,00 mm (valores didaticos).
  Catalogo > Comparar > A > B mostra A menos B = +0,20 mm.
  Inverter os registros muda o sinal. Zero significa medida nao informada.
  Use as medidas do seu instrumento; o desenho nao e uma regua fisica 1:1.

Exemplo em Counter:
  Mantenha Simulacao ativa, escolha 10 s, inicie e experimente Parar.
  Aguarde o estado final e use Salvar resultado para persistir o resumo.
  As metricas simuladas sao artificiais, nao observacoes da sua rede.
  Em ICMP real, 1 sonda sem resposta entre 20 concluidas significa 5% de perda.
  Latencia considera respostas recebidas; bloqueio ICMP tambem causa ausencia.
)HELP";
}
