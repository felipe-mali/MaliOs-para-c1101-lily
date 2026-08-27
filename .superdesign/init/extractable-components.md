# Extractable components

The source is a static monolithic document. These are extraction candidates for design consistency, not existing framework components.

## AppHeader

- Source: `embedded_resources/web_interface/index.html`
- Category: layout
- Description: Brand/version bar with primary navigation, settings and logout.
- Extractable props: `activeView`, `firmwareVersion`.
- Hardcoded: icon SVGs, action labels and global CSS classes.

## StorageSelector

- Source: `embedded_resources/web_interface/index.html`
- Category: basic
- Description: SD and LittleFS usage buttons that switch the active filesystem.
- Extractable props: `activeFilesystem`, `sdUsage`, `littleFsUsage`.
- Hardcoded: storage labels and CSS classes.

## ToolCard

- Source: new design pattern; derived from `.btn-action` and `.container` visual language.
- Category: basic
- Description: Compact dashboard entry for Files, QR Studio, Wi-Fi, Portal Studio and System.
- Extractable props: `title`, `summary`, `href`, `status`.
- Hardcoded: card geometry and local icon style.

## DialogShell

- Source: `embedded_resources/web_interface/index.html`
- Category: basic
- Description: Repeated modal head/body/footer structure used by settings, navigator, editor and prompts.
- Extractable props: `title`, `isOpen`, `size`.
- Hardcoded: close behavior conventions and global CSS classes.

## FileExplorer

- Source: `embedded_resources/web_interface/index.html`
- Category: basic
- Description: Breadcrumb, file actions and responsive file table.
- Extractable props: `filesystem`, `path`, `entries`.
- Hardcoded: supported action icons and endpoint conventions.

## StatusMetric

- Source: new design pattern for requested Dashboard/Wi-Fi views.
- Category: basic
- Description: Label/value/status unit that can explicitly render `Não verificado`.
- Extractable props: `label`, `value`, `tone`.
- Hardcoded: label typography and status-color semantics.
