"""Read-only release checks for the current sources and embedded WebUI."""
from pathlib import Path
import collections, glob, gzip, hashlib, re
ROOT=Path(__file__).resolve().parents[2]
web=ROOT/'embedded_resources/web_interface'
digest=hashlib.sha256()
for extension in ['html','css','js']:
    for filename in glob.glob(str(web/('*.'+extension))):
        digest.update(hashlib.sha256(Path(filename).read_bytes()).hexdigest().encode())
assert digest.hexdigest()==(web/'checksum.sha256').read_text().strip(), 'Rebuild embedded WebUI'
header=(ROOT/'include/webFiles.h').read_text(encoding='utf-8')
embedded={}
for name in ['index.html','index.js']:
    match=re.search(r'const uint8_t '+name.replace('.','_')+r'\[\] PROGMEM = \{([\s\S]*?)\};',header)
    assert match,name
    embedded[name]=gzip.decompress(bytes(int(h,16) for h in re.findall(r'0x([0-9a-fA-F]+)',match.group(1)))).decode('utf-8')
assert 'mali-tools-help' in embedded['index.html'] and '/MaliKeys/' in embedded['index.html']
for english in ['Fetching content...', 'Fetching system info...', 'Filename cannot be empty.', 'Username and password cannot be empty.', 'Rebooting...']:
    assert english not in embedded['index.js'],english
wiki=(ROOT/'src/modules/mali/MaliWiki.cpp').read_text(encoding='utf-8')
counts=collections.Counter(re.findall(r'WIKI_ENTRY\(\s*(\w+),',wiki));counts.pop('category_',None)
assert counts['MALI_KEYS']==7 and counts['MALI_COUNTER']>=3
menu=(ROOT/'src/core/menu_items/MaliToolsMenu.cpp').read_text(encoding='utf-8')
assert 'MaliKeys::open' in menu and 'KeyGauge::open' in menu
assert '/MaliTools/KeyGauge' in (ROOT/'src/mali_tools/key_gauge/KeyGaugeStore.cpp').read_text()
print('PASS: embedded source checksum, translated Web messages, help coverage, new and legacy key entry points')
print('Help pages:',sum(counts.values()),dict((k,v) for k,v in counts.items() if k.startswith('MALI')))
