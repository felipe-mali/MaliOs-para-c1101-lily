# MaliOS WebUI design system

## Product constraints

- Target: ESP32-S3-hosted local WebUI for desktop and mobile browsers.
- Stack: standalone HTML, vanilla CSS and vanilla JavaScript; no external fonts, scripts, images, CDNs or runtime services.
- Preserve every current file-manager, script-editor, TFT navigator, settings and authentication capability.
- Optimize for small compressed assets and low transient heap use.

## Identity

- Product name: `MALI OS`.
- Tone: restrained technical instrument panel, clear enough for non-experts.
- Primary palette: black/near-black surfaces with burgundy accents. Avoid coral, orange, salmon and bright red.
- Suggested semantic tokens: canvas `#070708`, surface `#101013`, raised surface `#171319`, border `#44212f`, primary burgundy `#7A1737`, primary hover `#982249`, readable accent `#C65A7D`, text `#F4EDF0`, muted text `#A8959D`, success `#43B581`, warning `#D7A84A`, danger `#C64A5D`.
- Keep runtime compatibility with `--color`, `--sec-color` and `--background`; additional variables may derive from them with safe CSS fallbacks.

## Typography and density

- Use the existing local monospace system stack for technical values and controls.
- Establish a compact scale: 12px metadata, 14px controls/body, 18–20px section titles, 24px product heading.
- Minimum touch target: 42px on narrow screens.
- No decorative animation; at most short color/focus transitions.

## Layout

- Responsive app shell with a compact persistent brand/header and an obvious active view.
- Desktop: left navigation or compact top navigation plus a content column capped for readability.
- Mobile: wrap/collapse navigation into large, full-width view buttons; never require horizontal page scrolling.
- Home/dashboard uses practical cards for Arquivos, QR Studio, Wi-Fi, Portal Studio, Scripts and Sistema.
- Preserve dialogs for focused actions; long forms use sections and explicit validation feedback.

## Components

- Buttons: solid burgundy primary, outlined neutral secondary, restrained destructive variant.
- Cards: near-black raised surface, 1px burgundy-tinted border, 8–10px radius, no heavy shadow.
- Inputs: dark fill, clear border, visible burgundy/pale focus ring, associated labels and help text.
- Status: never encode meaning with color alone; show text such as `Conectado`, `Desconectado`, `Ativo`, `Não verificado`.
- QR preview: high-contrast white square canvas with quiet zone; payload summary beside/below it, sensitive fields masked.
- File explorer: keep table semantics on desktop and compact row/card behavior on mobile.

## Requested information architecture

- Dashboard
- Arquivos
- QR Studio: PIX, Wi-Fi, URL, Texto, Telefone, E-mail, Contato/vCard, Favoritos, Histórico
- Wi-Fi
- Portal Studio
- Scripts
- Sistema

## Safety and privacy cues

- Show that QR generation is local.
- Wi-Fi favorite saving defaults to not persisting passwords and requires an explicit opt-in warning.
- Portal Studio labels the default page as `MALI LAB` and avoids third-party branding or credential language.
- Destructive actions require a confirmation and state the affected local target.
