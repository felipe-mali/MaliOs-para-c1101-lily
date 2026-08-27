# Shared UI components

## Framework and component model

- Framework: none; the embedded WebUI is static HTML, vanilla JavaScript and vanilla CSS.
- Component library: none.
- CSS approach: a single global stylesheet plus three runtime color variables.

There is no shared component source directory and no reusable JavaScript component abstraction to copy here. The current page-specific primitives (`.btn-action`, `.icon-action`, `.dialog`, form controls and table rows) are declared in `embedded_resources/web_interface/index.css` and instantiated directly by `embedded_resources/web_interface/index.html`. Their complete CSS implementation is recorded in `theme.md`.
