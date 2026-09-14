"""Apply the reviewed source catalog; never translate network responses or user data."""
from pathlib import Path
import re
from review_catalog import ROOT

catalog={}
for line in (ROOT/'src/core/ui/web_pt_br.tsv').read_text(encoding='utf-8').splitlines():
    if line and not line.startswith('#'):
        key,value=line.split('\t',1)
        assert key not in catalog,key
        catalog[key]=value

for path in (ROOT/'embedded_resources/web_interface').glob('*.html'):
    source=path.read_text(encoding='utf-8')
    def node(m):
        text=m.group(1);key=re.sub(r'\s+',' ',text.strip())
        value=catalog.get(key,catalog.get(text.strip()))
        if value is None:return m.group()
        return '>'+text[:len(text)-len(text.lstrip())]+value+text[len(text.rstrip()):]+'<'
    source=re.sub(r'>([^<>]+)<',node,source)
    def attr(m):return m.group(1)+'="'+catalog.get(m.group(2),m.group(2))+'"'
    source=re.sub(r'(title|aria-label|placeholder|alt)="([^"]*)"',attr,source)
    path.write_text(source,encoding='utf-8')

path=ROOT/'embedded_resources/web_interface/index.js'
source=path.read_text(encoding='utf-8')
# These are wire/API states, methods or metric keys, not labels at their definition sites.
protected={'DELETE','IDLE','LOW','MEDIUM','HIGH','CUSTOM','MODE','DURATION','START','SCAN','HISTORY','READY'}
for key,value in sorted(catalog.items(),key=lambda x:-len(x[0])):
    if key in protected:continue
    for quote in ['"',"'",'`']:
        if quote in key or quote in value:continue
        source=source.replace(quote+key+quote,quote+value+quote)
source=source.replace('>Loading...</td>', '>Carregando...</td>')
source=source.replace(' · LEVEL ${',' · NÍVEL ${').replace('`P${i+1} level`','`Nível de P${i+1}`')
source=source.replace('NETWORK COUNT ${','REDES ${').replace('AVG RSSI ${','RSSI MÉDIO ${').replace('ACTIVITY: visibilidade passiva','ATIVIDADE: visibilidade passiva').replace('ACTIVITY: AP visibility (max 32)','ATIVIDADE: visibilidade dos APs (máx. 32)')
source=source.replace("'SIMULATION · '","'SIMULAÇÃO · '").replace('`TARGET: ${','`ALVO: ${')
source=source.replace('${h.events} events · OK ${h.success} / FAIL ${h.failures}','${h.events} eventos · OK ${h.success} / FALHAS ${h.failures}')
source=source.replace('Use BACK/encoder no dispositivo para STOP.','Use VOLTAR/encoder no dispositivo para PARAR.').replace('STOP sem confirmacao:', 'Parada sem confirmação:').replace('Use BACK/encoder no dispositivo.', 'Use VOLTAR/encoder no dispositivo.')
source=source.replace('Nao foi possivel carregar o Dashboard.', 'Não foi possível carregar o painel.')
path.write_text(source,encoding='utf-8')
