"""Source invariants for reachability, UI translation and external contracts."""
import re, subprocess
from pathlib import Path
from review_catalog import ROOT

def old(path, revision='HEAD'):
    return subprocess.check_output(['git','show',f'{revision}:{path}'],cwd=ROOT).decode('utf-8-sig')
def current(path):return (ROOT/path).read_text(encoding='utf-8-sig')
def registered(text):
    block=text.split('_menuItems = {',1)[1].split('};',1)[0]
    return set(re.findall(r'&(\w+)',block))
menu=current('src/core/main_menu.cpp')
for revision in ['HEAD','e7265ac7']:
    assert registered(old('src/core/main_menu.cpp',revision)) <= registered(menu), 'Registered menu removed'
assert '&connectMenu' in menu and 'MaliUI::categoryFor(name.c_str()) == category' in menu
assert 'openCategory(3)' in menu and 'CounterLab::open' in menu
# Every declared root menu instance is in the traversed registry, including compile-gated modules.
instances=set(re.findall(r'^\s+\w+Menu (\w+);',current('src/core/main_menu.h'),re.M))
assert instances <= registered(menu), instances-registered(menu)
for path in (ROOT/'src/core/menu_items').glob('*.cpp'):
    before=old(path.relative_to(ROOT).as_posix());after=path.read_text(encoding='utf-8')
    calls=lambda s:set(re.findall(r'\b([A-Za-z_]\w*)\s*\(',s))
    missing=calls(before)-calls(after)-{'getName'}
    assert not missing,(str(path),missing)

for filename in ['KeyGaugeWebApi.cpp','CounterWebApi.cpp']:
    path='src/core/wifi/'+filename
    endpoints=lambda s:set(re.findall(r'server.on\("([^"]+)",\s*(HTTP_\w+)',s))
    assert endpoints(old(path))==endpoints(current(path)),filename
state=re.search(r'const char \*stateName\(State s\)\{[^\n]+',current('src/mali_tools/counter/CounterLab.cpp')).group()
assert all('"'+value+'"' in state for value in ['IDLE','SCANNING','CONFIGURING','RUNNING','STOPPING','COMPLETE','STOPPED','ERROR'])
js=current('embedded_resources/web_interface/index.js')
assert "['RUNNING','SCANNING','STOPPING'].includes(s.state)" in js
assert "'DELETE');" in js and 'maliStatusLabel(s.pending?' in js
assert 'label.textContent=maliStatusLabel(name)' in js and 'box.dataset.metric=name' in js
assert current('src/mali_tools/key_gauge/KeyGaugeStore.h')==old('src/mali_tools/key_gauge/KeyGaugeStore.h'), 'Profile format changed'
assert not subprocess.check_output(['git','diff','--name-only','--','lib','boards','src/modules/rf/protocols'],cwd=ROOT).strip(), 'Driver or RF protocol changed'
# Detect lost definitions, not just declarations: a forward declaration must not
# hide an accidentally removed UI confirmation or tool implementation.
definitions=lambda s:set(re.findall(r'^\s*(?:void|bool|int|int8_t|uint8_t|String)\s+([\w:]+)\s*\([^;{}]*\)\s*\{',s,re.M))
for name in subprocess.check_output(['git','diff','--name-only','--','src'],cwd=ROOT).decode().splitlines():
    if name.endswith('.cpp'):
        missing=definitions(old(name))-definitions(current(name))
        assert not missing,(name,missing)
print(f'PASS: {len(instances)} registered menus, previous callbacks, routes/methods, API states, profile format and drivers preserved')
