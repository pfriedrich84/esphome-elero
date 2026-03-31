---
name: build-web-ui
description: "Rebuild the Elero web UI frontend (Vite + Alpine.js) and regenerate elero_web_ui.h. Use after editing frontend source files."
user-invocable: true
---

# Build Web UI

Rebuild the Elero web UI frontend and regenerate the embedded C++ header.

## Steps

1. `cd components/elero_web/frontend`
2. If `node_modules/` is missing: `npm install`
3. `npm run build` (runs Vite build + `scripts/generate_header.mjs`)
4. Verify `../elero_web_ui.h` was updated (check file timestamp)
5. Report bundle size and any warnings

## Build Pipeline

```
frontend/src/ (HTML + JS + CSS)
  → Vite build with vite-plugin-singlefile
  → dist/index.html (single file, CSS/JS inlined)
  → generate_header.mjs
  → elero_web_ui.h (C++ PROGMEM raw string literal)
```

## Important

- `elero_web_ui.h` is auto-generated — never edit by hand
- Both frontend source AND the generated header must be committed together
- For development with hot-reload: `npm run dev` (starts Vite dev server)
