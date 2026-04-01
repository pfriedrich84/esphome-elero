/**
 * Serializing request queue: max 1 in-flight request at a time.
 * Prevents ESP32 socket exhaustion (ENFILE error 23).
 */
const queue = []
let busy = false

async function drain() {
  if (busy) return
  while (queue.length > 0) {
    busy = true
    const { method, fullUrl, resolve, reject } = queue.shift()
    try {
      const r = await fetch(fullUrl, { method, signal: AbortSignal.timeout(10000) })
      if (!r.ok) {
        let msg = `HTTP ${r.status}`
        try { const d = await r.json(); msg = d.error || msg } catch {}
        reject(new Error(msg))
      } else {
        const ct = r.headers.get('content-type') || ''
        resolve(ct.includes('application/json') ? await r.json() : await r.text())
      }
    } catch (e) { reject(e) }
    busy = false
  }
}

export function api(method, url, params = {}) {
  const qs = Object.keys(params).length
    ? '?' + new URLSearchParams(params).toString() : ''
  return new Promise((resolve, reject) => {
    queue.push({ method, fullUrl: url + qs, resolve, reject })
    drain()
  })
}
