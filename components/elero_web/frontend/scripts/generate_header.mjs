/**
 * generate_header.mjs
 * Reads dist/index.html produced by Vite, gzip-compresses it,
 * and writes ../elero_web_ui.h as a C byte array for the ESP32 firmware.
 *
 * Serving gzipped content saves ~60% flash vs raw string literals.
 */
import fs from 'fs'
import path from 'path'
import { fileURLToPath } from 'url'
import { gzipSync } from 'zlib'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const distDir   = path.resolve(__dirname, '..', 'dist')
const outFile   = path.resolve(__dirname, '..', '..', 'elero_web_ui.h')
const htmlFile  = path.join(distDir, 'index.html')

if (!fs.existsSync(htmlFile)) {
  console.error('ERROR: dist/index.html not found — run vite build first')
  process.exit(1)
}

const html = fs.readFileSync(htmlFile)
const compressed = gzipSync(html, { level: 9 })

// Format as C byte array
const bytes = []
for (let i = 0; i < compressed.length; i++) {
  bytes.push(`0x${compressed[i].toString(16).padStart(2, '0')}`)
}

// Wrap lines at 16 bytes for readability
const lines = []
for (let i = 0; i < bytes.length; i += 16) {
  lines.push('    ' + bytes.slice(i, i + 16).join(', '))
}

const header = `#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __AVR__
#include <pgmspace.h>
#elif !defined(PROGMEM)
#define PROGMEM
#endif

// AUTO-GENERATED FILE — do not edit by hand.
// To rebuild: cd components/elero_web/frontend && npm run build

namespace esphome {
namespace elero {

// gzip-compressed HTML (${html.length} bytes raw → ${compressed.length} bytes gzipped)
const uint8_t ELERO_WEB_UI_GZ[] PROGMEM = {
${lines.join(',\n')}
};

const size_t ELERO_WEB_UI_GZ_SIZE = sizeof(ELERO_WEB_UI_GZ);

}  // namespace elero
}  // namespace esphome
`

fs.writeFileSync(outFile, header, 'utf8')
console.log(`Written ${html.length} bytes HTML → ${compressed.length} bytes gzipped → ${outFile}`)
