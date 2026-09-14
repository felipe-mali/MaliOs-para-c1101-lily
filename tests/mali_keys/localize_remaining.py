"""Reviewed remaining UI translations; never edits commands, enum IDs or payload templates."""
from pathlib import Path
import hashlib, json, re, subprocess, sys
ROOT = Path(__file__).resolve().parents[2]
groups = {
    'src/mali_tools/counter/counter_main.cpp': {
        'ENC:pag OK:salvar BACK:sair':'ENC:pag OK:salvar VOLTAR',
        'ENC:pagina BACK:sair':'ENC:pagina VOLTAR:sair'},
    'src/core/mykeyboard.h': {'Type you HEX value:':'Digite o valor HEX:', 'Insert your number:':'Digite o numero:'},
    'src/core/display.h': {'Running, Wait':'Executando, aguarde'},
    'src/modules/others/clicker.cpp': {
        'Initializing USB HID...':'Iniciando USB HID...', 'Please wait':'Aguarde',
        'LEFT':'ESQ.','RIGHT':'DIR.','MID':'MEIO','Infinite':'Infinito','Clicks/Second':'Cliques/segundo',
        '-> CLICKING <-':'-> CLICANDO <-','-> CLICK <-':'-> CLIQUE <-','USB INIT':'INICIANDO USB'},
    'src/modules/rf/rf_utils.cpp': {'Fixed [':'Fixa [', 'Choose Fixed':'Escolher fixa','All ranges':'Todas as faixas'},
    'src/modules/rfid/RFIDInterface.h': {
        'Success':'Sucesso','Failed reading data blocks':'Falha ao ler blocos de dados',
        'Failed reading. Tag not found':'Falha na leitura. Tag nao encontrada',
        "Error! Tags don't match":'Erro! Tags diferentes', 'Failed authenticating':'Falha na autenticacao',
        'Not implemented':'Nao implementado'},
    'src/modules/badusb_ble/ducky_typer.cpp': {
        'ScreenShot':'Capturar tela', 'Play/Pause':'Reproduzir/Pausar', 'Prev Track':'Faixa anterior',
        'Hold Vol +':'Manter volume +','Mute':'Silenciar'},
    'src/modules/wifi/netcut.cpp': {'s OFF / ':'s DESL. / ', 's ON':'s LIG.', 'off/':'desl./', 'on':'lig.'},
    'src/core/wifi/webInterface.cpp': {
        'Invalid or unavailable file system':'Armazenamento invalido ou indisponivel',
        'Invalid destination path':'Caminho de destino invalido','Invalid encryption password':'Senha de criptografia invalida',
        'Invalid upload filename':'Nome do arquivo de envio invalido','Invalid upload path':'Caminho de envio invalido',
        'Failed to open upload destination':'Falha ao abrir destino do envio',
        'Failed to encrypt or write upload':'Falha ao criptografar ou gravar envio','Failed to write upload':'Falha ao gravar envio',
        'Invalid file system or path':'Armazenamento ou caminho invalido',
        'command failed, check the serial log for details':'Falha no comando; consulte o registro serial',
        'http request missing required arg: cmnd':'Requisicao HTTP sem o argumento obrigatorio: cmnd',
        'Invalid file system or folder':'Armazenamento ou pasta invalida','Folder not found':'Pasta nao encontrada',
        'Invalid file system, path or action':'Armazenamento, caminho ou acao invalida',
        'Failed to open file for reading':'Falha ao abrir arquivo para leitura','ERROR: invalid action param supplied':'ERRO: parametro action invalido',
        'Invalid file system, path or content size':'Armazenamento, caminho ou tamanho invalido',
        'Failed to write to file: ':'Falha ao gravar arquivo: ','Failed to open file for writing: ':'Falha ao abrir arquivo para gravacao: ',
        'Invalid upload request':'Requisicao de envio invalida'},
}

def run():
    catalog_path=ROOT/'src/core/ui/pt_br.tsv'
    original=catalog_path.read_text(encoding='utf-8')
    catalog=dict(line.split('\t',1) for line in original.splitlines() if line and not line.startswith('#'))
    additions=[]
    for translations in groups.values():
        for english,portuguese in translations.items():
            if english not in catalog:
                additions.append(english+'\t'+portuguese);catalog[english]=portuguese
    if additions:catalog_path.write_text(original.rstrip('\n')+'\n'+'\n'.join(additions)+'\n',encoding='utf-8')
    for relative,translations in groups.items():
        path=ROOT/relative;source=path.read_text(encoding='utf-8')
        before=source
        for english in translations:
            stem=re.sub(r'[^a-z0-9]+','_',english.lower()).strip('_')[:44]
            if not stem or stem[0].isdigit():stem='text_'+stem
            symbol='MaliText::'+stem+'_'+hashlib.sha1(english.encode()).hexdigest()[:6]
            literal=json.dumps(english)
            source=source.replace('F('+literal+')',symbol).replace(literal,symbol)
        if source!=before:
            if '#include "core/ui/PtBr.h"' not in source:source='#include "core/ui/PtBr.h"\n'+source
            path.write_text(source,encoding='utf-8')
    subprocess.run([sys.executable,str(ROOT/'tests/mali_ui/migrate_locale.py')],check=True)
if __name__=='__main__':run()
