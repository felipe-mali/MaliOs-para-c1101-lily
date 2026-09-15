"""Integration checks supplement the executable model/protocol/storage tests."""
from pathlib import Path
import json,re,subprocess
ROOT=Path(__file__).resolve().parents[2]
module=ROOT/'src/modules/wifi/inspector'
old_menu=subprocess.check_output(['git','show','HEAD:src/core/menu_items/WifiMenu.cpp'],cwd=ROOT).decode('utf-8')
menu=(ROOT/'src/core/menu_items/WifiMenu.cpp').read_text(encoding='utf-8')
calls=lambda s:set(re.findall(r'\b([\w:]+)\s*\(',s))
assert calls(old_menu)<=calls(menu)
assert '"Wi-Fi Inspector", WifiInspector::open' in menu
scanner=(module/'device_scanner.cpp').read_text(encoding='utf-8')
sources='\n'.join(p.read_text(encoding='utf-8') for p in module.glob('*.cpp'))
for prohibited in ['esp_wifi_80211_tx','etharp_cleanup_netif','wifiDisconnect(','WiFi.disconnect(','esp_wifi_set_promiscuous','xTaskCreate','MDNS.begin','MDNS.end','http.GET','setInsecure']:
    assert prohibited not in sources,prohibited
assert 'O_NONBLOCK' in scanner and '90000' in scanner and 'sock.close()' in scanner and 'udp.stop()' in scanner
assert 'LOCK_TCPIP_CORE();etharp_request' in scanner
assert 'std::array<Device,MAX_DEVICES>' in (module/'device_model.h').read_text()
oui=json.loads((ROOT/'tests/wifi_inspector/oui_source.json').read_text(encoding='utf-8'))
assert len(set(e['vendor'] for e in oui['entries']))==20
assert len(set(e['prefix'] for e in oui['entries']))==len(oui['entries'])
assert all(not(int(e['prefix'][:2],16)&3) for e in oui['entries'])
compiled=(module/'oui_database.cpp').read_text()
assert all('0x'+e['prefix'] in compiled for e in oui['entries'])
wiki=(ROOT/'src/modules/mali/MaliWiki.cpp').read_text(encoding='utf-8')
entries=re.findall(r'WIKI_ENTRY\(\s*(\w+),\s*"([^"]+)"([\s\S]*?)\n    \)',wiki)
old_wiki=subprocess.check_output(['git','show','HEAD:src/modules/mali/MaliWiki.cpp'],cwd=ROOT).decode('utf-8')
old_names=set((c,n) for c,n,_ in re.findall(r'WIKI_ENTRY\(\s*(\w+),\s*"([^"]+)"([\s\S]*?)\n    \)',old_wiki))
assert old_names<=set((c,n) for c,n,_ in entries)
assert sum(c=='WIFI_INSPECTOR' for c,_,_ in entries)==8
for category,name,body in entries:
    assert 'Situacao:' in body and 'Resultado:' in body,(category,name)
print(f'PASS: old Wi-Fi callbacks preserved; bounded scanner and no radio attack path; {len(oui["entries"])} OUI prefixes / 20 vendors; {len(entries)} practical help pages')
