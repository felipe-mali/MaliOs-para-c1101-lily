# Route map

Routing is registered in `configureWebServer()` in `src/core/wifi/webInterface.cpp` using ESPAsyncWebServer. There is no client-side router configuration.

## Documents and static resources

| URL | Method | Source | Authentication | Purpose |
| --- | --- | --- | --- | --- |
| `/` | GET | `embedded_resources/web_interface/index.html` | Session cookie; otherwise login page | Authenticated WebUI shell and file manager |
| `/login` | POST | inline handler + `login.html` | Credential verification | Starts a WebUI session |
| `/logout` | GET | inline handler | Session cleanup | Ends a WebUI session |
| `/index.css` | GET | `embedded_resources/web_interface/index.css` | Public static asset | Global UI styles |
| `/theme.css` | GET | generated from MaliOS display colors or `/BruceWebUI/theme.css` | Public static asset | Runtime theme variables |
| `/index.js` | GET | `embedded_resources/web_interface/index.js` | Public static asset | WebUI behavior |

## Authenticated API routes

`/systeminfo`, `/getscreen`, `/rename`, `/cm`, `/reboot`, `/listfiles`, `/file`, `/edit`, `/upload` (upload callback requires an authentication audit) and `/wifi`.

The design work may add navigation views inside the existing authenticated `/` shell, but server APIs remain registered in `configureWebServer()` and must preserve `checkUserWebAuth()`.
