"""Inventory UI literals without touching identifiers, payloads or file formats."""
import json, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
CALL = re.compile(r'(?:\b(?:tft|sprite|spr|_scrollBuffer|display|canvas|screen|p|out)\s*[.>]\s*(?:print|println|printf|drawString|drawCentreString|drawCenterString|drawRightString|text)|\b(?:displayError|displayInfo|displayWarning|displaySuccess|displayMessage|displayTextLine|displayText|displayRedStripe|drawMainBorderWithTitle|printTitle|printSubtitle|printFootnote|drawHeader|drawFooter|drawDialog|drawToast|drawMenuItem|drawButton|keyboard|loopOptions|padprint|padprintln|textLine|label|metric|showAttackProgress|showAttackResult|showWarningMessage|showErrorMessage|showSuccessMessage|showAdaptiveMessage|requireSimpleConfirmation|showSubMenu))\s*\(')

def scan(path):
    source = path.read_text(encoding='utf-8-sig')
    matches = list(TOKEN.finditer(source))
    masked = list(source)
    for m in matches:
        if not m.group().startswith('"'):
            masked[m.start():m.end()] = ' ' * (m.end()-m.start())
    clean = ''.join(masked)
    # Parentheses are masked too, so strings cannot change the call boundaries.
    chars = list(clean)
    for m in matches:
        chars[m.start():m.end()] = ' ' * (m.end()-m.start())
    bare = ''.join(chars)
    spans = []
    for call in CALL.finditer(clean):
        depth, end = 1, call.end()
        while end < len(bare) and depth:
            if bare[end] == '(': depth += 1
            elif bare[end] == ')': depth -= 1
            end += 1
        spans.append((call.start(),end))
    rows=[]
    for m in matches:
        if not m.group().startswith('"'): continue
        raw=m.group()[1:-1]
        if not re.search('[a-zA-Z]',raw): continue
        before, after=clean[max(0,m.start()-100):m.start()],clean[m.end():m.end()+100]
        menu=bool(re.search(r'\{\s*$',before) and re.match(r'\s*,\s*(?:\[|[A-Za-z_]\w*\s*(?:\(|,|\}))',after))
        ui=menu or any(a<=m.start()<b for a,b in spans)
        rows.append(dict(text=raw,path=path.relative_to(ROOT).as_posix(),line=source.count('\n',0,m.start())+1,start=m.start(),end=m.end(),ui=ui))
    return rows

if __name__=='__main__':
    rows=[r for folder in ['src','include'] for path in (ROOT/folder).rglob('*') if path.suffix in ['.cpp','.h'] and path.name not in ['webFiles.h','PtBr.h'] for r in scan(path)]
    out=ROOT/'tests/mali_ui/review-inventory.json'
    out.write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
    texts=sorted(set(r['text'] for r in rows if r['ui']))
    (out.parent/'review-strings.txt').write_text('\n'.join(texts),encoding='utf-8')
    print(f'{len(rows)} literals, {sum(r["ui"] for r in rows)} UI occurrences, {len(texts)} unique UI strings')
