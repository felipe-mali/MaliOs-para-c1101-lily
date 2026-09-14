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
  Ferramentas > Mali Tools > Chaves: plana, cruciforme, medicao em mm e catalogo /MaliKeys/.
  Chaves > Catalogo > Comparar: abre dois registros; diferencas usam A menos B.
  Chaves > ? Ajuda: medidas, quatro faces, texto, salvar, carregar, renomear e excluir.
  KEY GAUGE: editor anterior preservado, com arquivos .mkg separados do catalogo .mkey.
  Counter: testes de resiliencia, simulacao, metricas e parada pela tela ou WebUI.
  D20 e Pixel Paint: dados e desenho local em Ferramentas.
  Encoder: girar escolhe, clicar confirma, segurar volta.
)HELP";
}
