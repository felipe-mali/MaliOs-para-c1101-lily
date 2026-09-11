# KEY GAUGE

Entry: Mali Tools > KEY GAUGE. Uses the existing display/theme, input flags,
menu, keyboard and filesystem selection (mounted SD, otherwise LittleFS).

Controls:
- NEW PROFILE starts with 6 points selected; choose 4..10.
- In EDIT/GUIDE, encoder left decreases the selected level, right increases it (0..9).
- Click advances the point. BACK opens options, following the T-Embed convention.
- PREVIOUS POINT in options selects the preceding point.
- THICKNESS: encoder adjusts 1..10 (default 5); click confirms and returns.
- PROFILE WIDTH: encoder adjusts 50..100% (default 90%); click confirms and returns.
- PROFILE restores the regular view; GUIDE hides status and body fill.
- SAVE AS uses the existing keyboard, proposes the first unused PROFILE_001 etc.
- Exit without saving discards the in-memory edit. Saving creates a new file.

THICKNESS moves only the body's lower edge. The upper contour's pixel positions
and levels do not change. WIDTH changes only horizontal coordinates. Neither
parameter represents commercial dimensions, manufacturers or key codes.
Calibration is a separate adjustable reference bar, saved as a relative factor
in calibration.txt. It is not used to convert profile geometry to measurements.

Profiles: /MaliTools/KeyGauge/<name>.mkg on the selected filesystem:

```text
MKG1
name=PROFILE_001
points=6
thickness=5
width=90
levels=2,2,5,3,6,1
```

MKG1 files without thickness/width load with defaults 5/90. Invalid fields,
duplicate fields, invalid counts/levels and files over 1024 bytes are rejected.
Loading parses into a temporary profile before committing. Saves write a temporary
file and rename it; existing named profiles are not overwritten. Deletion has a
confirmation menu. There is no STL, cutting workflow or manufacturer database.

Build in PowerShell:

```powershell
& 'C:\Users\felip\.platformio\penv\Scripts\pio.exe' run -e lilygo-t-embed-cc1101
```

Hardware verification still required: encoder orientation, readability at 10
points, and drawing extremes in normal/guide modes on the physical display.

## WebUI expansion

Open the existing WebUI and choose **Mali Tools / KEY GAUGE** (also a home card).
The page lives in the existing index.html/index.css/index.js, is embedded by the
existing patch.py gzip step, and uses the same authenticated AsyncWebServer.
No server, database, runtime framework or CDN was added.

Shared storage/validation is in KeyGaugeStore.{h,cpp}; both the local app and API
use it. The mounted SD is selected, otherwise LittleFS, just as before. Files stay
in /MaliTools/KeyGauge with the unchanged MKG1 representation. profileWidth in JSON
maps to width in MKG1. Legacy files retain the default thickness=5 and width=90.
Names are 1..31 ASCII letters/digits/underscore/hyphen; paths are never taken from
the request. The bounded list supports 256 profiles; additional creates return
507 while existing profiles remain editable/deletable. API writes and local
profile operations are serialized by a mutex.

Saving a new profile fails with 409 if the name exists. Saving an opened profile
uses explicit replace=1. To support SD/FAT, replacement writes and flushes .tmp,
renames the old file to .bak, then commits the temporary file and removes backup.
On failed commit the old file is restored. A leftover .bak is recovered on the
next list/read/write/delete. A closed browser cannot leave a partly written
.mkg: the server completes the operation independently of the page. A timeout
instructs the user to refresh the list before retrying. Physical SD removal or
media failure still requires hardware validation.

Authenticated routes (same cookie/authentication callback as the existing APIs):

| Method | Path | Request / response |
| --- | --- | --- |
| GET | /api/keygauge/profiles | {items:[names], nextName} |
| GET | /api/keygauge/profile?name=... | Profile JSON |
| POST | /api/keygauge/profile | Form profile=<JSON>, replace=0 or 1 |
| DELETE | /api/keygauge/profile?name=... | Delete saved profile |
| POST | /api/keygauge/preview | Form profile=<JSON>; 202, RAM only |

```json
{"name":"PROFILE_001","points":6,"levels":[2,2,5,3,6,1],"thickness":5,"profileWidth":90}
```

API failures return JSON {error:...}: 400 invalid input, 404 missing file,
409 name conflict, 413 oversized request, 422 corrupt profile, 503 unavailable/busy
storage, 507 capacity/write failure. POST bodies are limited to 2048 encoded
bytes and 1024 JSON bytes. Types and integer ranges are checked before narrowing.

**SHOW ON DEVICE:** the HTTP callback copies a validated profile into a protected
RAM slot. The foreground WebUI loop consumes it and draws on the TFT using the
existing KEY GAUGE renderer. It does not save or enter a nested app loop from
AsyncTCP. While another app/menu owns the screen, the RAM preview is available at
Mali Tools > KEY GAUGE > WEB PREVIEW. New previews replace the RAM slot; they do
not overwrite a locally edited profile. Reboot discards the slot.

**Browser editing:** Canvas uses Pointer Events with pointer capture, including
mouse and touchscreen. The nearest marker column has a generous touch hit area;
vertical drag rounds to 0..9 and clamps outside the canvas. pointercancel,
lostpointercapture and window blur end the drag. touch-action:none on the canvas
prevents page scrolling during touch editing. Labelled range controls also work
with the keyboard. Only SAVE sends a persistent write. DUPLICATE creates an
unsaved copy with an automatically available name. Unsaved changes are marked,
with confirmation before replacing the working copy or leaving the page.

**Device UX review:** added a separate triangle above the selected marker,
point/level text in GUIDE, explicit save success feedback, a bounded title for
long names, accurate VIEW/WEB hints and direct BACK from VIEW. Existing edit,
calibration, guide and save-as flows remain available.

UX reference reviewed after integration: https://lab.flipper.net/apps/key_copier
and the public MIT repository https://github.com/zinongli/KeyCopier (especially
the active-point indicator and left/right point selection in key_copier.c).
Only UX decisions were studied; no source code or Flipper APIs were copied.

## Verification

`node --check embedded_resources/web_interface/index.js`

Browser tests, using an installed Playwright and Chromium:

```powershell
# Set PLAYWRIGHT_MODULE if playwright is not resolvable through NODE_PATH.
node tests/key_gauge/web_test.cjs
$env:KEY_GAUGE_EMBEDDED='1'
node tests/key_gauge/web_test.cjs
```

The tests run the real source/embedded gzip assets with an in-memory HTTP fixture,
on desktop mouse and mobile touch. They check drag outside the canvas, clamping,
no implicit writes, independent scalar controls, loading/saving/duplication,
preview without persistence, invalid responses/names, safe deletion of the opened
profile and absence of horizontal overflow/runtime exceptions. PNG captures are
in tests/key_gauge/. These tests do not replace an end-to-end test of firmware
HTTP routes, filesystem failures, or the physical ST7789/encoder.
