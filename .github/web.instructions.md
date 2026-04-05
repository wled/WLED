---
applyTo: "wled00/data/**"
---
# Web UI Coding Conventions

## Formatting

- Indent **HTML and JavaScript** with **tabs**
- Indent **CSS** with **tabs**

## JavaScript Style

- **camelCase** for functions and variables: `gId()`, `selectedFx`, `currentPreset`
- Abbreviated helpers are common: `d` for `document`, `gId()` for `getElementById()`

## Key Files

- `index.htm` — main interface
- `index.js` — functions that manage / update the main interface
- `settings*.htm` — configuration pages
- `*.css` — stylesheets (inlined during build)
- `common.js` — helper functions 

## Build Integration

Files in this directory are processed by `tools/cdata.js` into `wled00/html_*.h` headers.
Run `npm run build` after any change. **Never edit the generated `html_*.h` files directly.**
