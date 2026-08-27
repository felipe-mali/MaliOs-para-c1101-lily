# Shared layouts

The current WebUI has no router framework and no layout component shared as source between pages.

- `/` uses the standalone document `embedded_resources/web_interface/index.html`.
- `/login` uses the standalone document `embedded_resources/web_interface/login.html`.

The authenticated document contains its own application shell: header, storage selector, file explorer, dialogs and TFT navigator. The login document independently reuses only global CSS class names. Full page source is passed directly as bounded Superdesign context for reproduction; there is no shared layout file whose contents could be duplicated here.
