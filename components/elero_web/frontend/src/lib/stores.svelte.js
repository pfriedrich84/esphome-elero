import { api } from './api.js'

/** Single reactive state object — Svelte 5 requires $state in an object for cross-module mutation */
export const s = $state({
  // Navigation
  tab: 'devices',
  // Device info
  deviceName: '',
  uptimeMs: 0,
  // Devices
  covers: [],
  lights: [],
  settingsOpen: null,
  // Discovery
  scanning: false,
  allDiscovered: [],
  // Adopt modal
  adoptTarget: null,
  adoptName: '',
  adoptType: 'cover',
  // YAML modal
  yamlContent: null,
  // Log
  logCapture: false,
  logLevel: 3,
  logAutoScroll: true,
  logEntries: [],
  logLastTs: 0,
  // Config
  freq: { freq2: '', freq1: '', freq0: '' },
  freqStatus: '',
  dumpActive: false,
  dumpPackets: [],
  // Toast
  toast: { show: false, error: false, msg: '' },
})

// ── Derived helpers ──────────────────────────────────────────────────────────
export function getDiscoveredNew() {
  return s.allDiscovered.filter(b => !b.already_configured && !b.already_adopted)
}
export function getDiscoveredKnown() {
  return s.allDiscovered.filter(b => b.already_configured || b.already_adopted)
}
export function getFilteredLog() {
  return s.logEntries.filter(e => e.level <= s.logLevel)
}

// ── Toast ────────────────────────────────────────────────────────────────────
let toastTimer = null
export function showToast(msg, isError = false) {
  if (toastTimer) clearTimeout(toastTimer)
  s.toast = { show: true, error: isError, msg }
  toastTimer = setTimeout(() => { s.toast.show = false }, 3500)
}

// ── Polling ──────────────────────────────────────────────────────────────────
let polling = false
let pollAttempts = 0

function schedulePoll() {
  const delay = pollAttempts > 0 ? Math.min(3000 * Math.pow(2, pollAttempts), 30000) : 3000
  setTimeout(async () => {
    if (polling) { schedulePoll(); return }
    polling = true
    try {
      await refreshStatus()
      pollAttempts = 0 // reset on success
    } catch {
      pollAttempts++
    }
    polling = false
    schedulePoll()
  }, delay)
}

export async function init() {
  await refreshInfo()
  try {
    await refreshStatus()
    pollAttempts = 0
  } catch {
    // Initial load failed — still schedule retry polling
    pollAttempts = 1
  }
  await loadFrequency()
  schedulePoll()
}

export async function refreshInfo() {
  try {
    const d = await api('GET', '/elero/api/info')
    s.deviceName = d.device_name || ''
    s.uptimeMs = d.uptime_ms || 0
    s.freq.freq2 = d.freq2 || s.freq.freq2
    s.freq.freq1 = d.freq1 || s.freq.freq1
    s.freq.freq0 = d.freq0 || s.freq.freq0
  } catch {}
}

export async function refreshStatus() {
  const params = { tab: s.tab }
  if (s.tab === 'log' && s.logLastTs) params.since = s.logLastTs
  const d = await api('GET', '/elero/api/status', params)

  s.covers = (d.covers || []).map(c => ({
    ...c,
    _edit: {
      open_duration_ms:  c.open_duration_ms,
      close_duration_ms: c.close_duration_ms,
      poll_interval_ms:  c.poll_interval_ms,
    }
  }))
  s.lights = (d.lights || []).map(l => ({
    ...l,
    _edit: { dim_duration_ms: l.dim_duration_ms }
  }))
  s.uptimeMs += 3000

  if (d.discovered !== undefined) {
    s.scanning = d.scanning
    s.allDiscovered = d.discovered || []
  }
  if (d.log_entries !== undefined) {
    s.logCapture = d.capture_active
    if (d.log_entries.length > 0) {
      const ne = d.log_entries.map((e, i) => ({ ...e, idx: s.logEntries.length + i }))
      s.logEntries = [...s.logEntries, ...ne]
      if (s.logEntries.length > 500) s.logEntries = s.logEntries.slice(s.logEntries.length - 500)
      s.logLastTs = ne[ne.length - 1].t
    }
  }
  if (d.packets !== undefined) {
    s.dumpActive = d.dump_active
    s.dumpPackets = d.packets || []
  }
}

// ── Cover/Light commands ─────────────────────────────────────────────────────
export async function coverCmd(c, cmd) {
  try {
    await api('POST', `/elero/api/covers/${c.blind_address}/command`, { cmd })
    showToast(`${c.name}: ${cmd} sent`)
  } catch (e) { showToast(`Command failed: ${e.message}`, true) }
}

export async function lightCmd(l, cmd) {
  try {
    await api('POST', `/elero/api/lights/${l.blind_address}/command`, { cmd })
    showToast(`${l.name}: ${cmd} sent`)
  } catch (e) { showToast(`Command failed: ${e.message}`, true) }
}

export async function saveSettings(c) {
  try {
    await api('POST', `/elero/api/covers/${c.blind_address}/settings`, {
      open_duration:  c._edit.open_duration_ms,
      close_duration: c._edit.close_duration_ms,
      poll_interval:  c._edit.poll_interval_ms,
    })
    showToast(`${c.name}: settings saved`)
    s.settingsOpen = null
  } catch (e) { showToast(`Save failed: ${e.message}`, true) }
}

// ── Discovery ────────────────────────────────────────────────────────────────
export async function startScan() {
  try {
    await api('POST', '/elero/api/scan/start')
    s.scanning = true
    showToast('Scan started')
  } catch (e) { showToast(`Scan start failed: ${e.message}`, true) }
}

export async function stopScan() {
  try {
    await api('POST', '/elero/api/scan/stop')
    s.scanning = false
    showToast('Scan stopped')
  } catch (e) { showToast(`Scan stop failed: ${e.message}`, true) }
}

export function startAdopt(b) {
  s.adoptTarget = b
  s.adoptName = ''
  s.adoptType = (b.last_state === 'on' || b.last_state === 'off') ? 'light' : 'cover'
}

export async function confirmAdopt() {
  if (!s.adoptTarget) return
  try {
    await api('POST', `/elero/api/discovered/${s.adoptTarget.blind_address}/adopt`,
              { name: s.adoptName || s.adoptTarget.blind_address, type: s.adoptType })
    showToast(`Adopted as "${s.adoptName || s.adoptTarget.blind_address}"`)
    s.adoptTarget = null
    s.tab = 'devices'
  } catch (e) { showToast(`Adopt failed: ${e.message}`, true) }
}

export function showYamlBlind(b) {
  const isLight = b.last_state === 'on' || b.last_state === 'off'
  if (isLight) {
    s.yamlContent =
      `light:\n` +
      `  - platform: elero\n` +
      `    blind_address: ${b.blind_address}\n` +
      `    channel: ${b.channel}\n` +
      `    remote_address: ${b.remote_address}\n` +
      `    name: "My Light"\n` +
      `    # dim_duration: 0s\n` +
      `    hop: ${b.hop}\n` +
      `    payload_1: ${b.payload_1}\n` +
      `    payload_2: ${b.payload_2}\n` +
      `    pck_inf1: ${b.pck_inf1}\n` +
      `    pck_inf2: ${b.pck_inf2}\n`
  } else {
    s.yamlContent =
      `cover:\n` +
      `  - platform: elero\n` +
      `    blind_address: ${b.blind_address}\n` +
      `    channel: ${b.channel}\n` +
      `    remote_address: ${b.remote_address}\n` +
      `    name: "My Blind"\n` +
      `    # open_duration: 25s\n` +
      `    # close_duration: 22s\n` +
      `    hop: ${b.hop}\n` +
      `    payload_1: ${b.payload_1}\n` +
      `    payload_2: ${b.payload_2}\n` +
      `    pck_inf1: ${b.pck_inf1}\n` +
      `    pck_inf2: ${b.pck_inf2}\n`
  }
}

export async function downloadYaml() {
  try {
    const text = await api('GET', '/elero/api/yaml')
    const blob = new Blob([text], { type: 'text/plain' })
    const url  = URL.createObjectURL(blob)
    const a    = document.createElement('a')
    a.href = url; a.download = 'elero_blinds.yaml'; a.click()
    URL.revokeObjectURL(url)
  } catch (e) { showToast(`YAML download failed: ${e.message}`, true) }
}

export function copyYaml() {
  navigator.clipboard?.writeText(s.yamlContent)
    .then(() => showToast('Copied!'))
    .catch(() => showToast('Copy failed', true))
}

// ── Log ──────────────────────────────────────────────────────────────────────
export async function startCapture() {
  try {
    await api('POST', '/elero/api/logs/capture/start')
    s.logCapture = true
    showToast('Log capture started')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

export async function stopCapture() {
  try {
    await api('POST', '/elero/api/logs/capture/stop')
    s.logCapture = false
    showToast('Log capture stopped')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

export async function clearLog() {
  try {
    await api('POST', '/elero/api/logs/clear')
    s.logEntries = []
    s.logLastTs = 0
    showToast('Log cleared')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

// ── Frequency ────────────────────────────────────────────────────────────────
export async function loadFrequency() {
  try {
    const d = await api('GET', '/elero/api/frequency')
    s.freq.freq2 = d.freq2
    s.freq.freq1 = d.freq1
    s.freq.freq0 = d.freq0
  } catch {}
}

export function applyPreset(v) {
  if (!v) return
  const [f2, f1, f0] = v.split(',')
  s.freq.freq2 = '0x' + f2
  s.freq.freq1 = '0x' + f1
  s.freq.freq0 = '0x' + f0
}

export async function setFrequency() {
  s.freqStatus = 'Applying...'
  try {
    const d = await api('POST', '/elero/api/frequency/set', { freq2: s.freq.freq2, freq1: s.freq.freq1, freq0: s.freq.freq0 })
    s.freq.freq2 = d.freq2
    s.freq.freq1 = d.freq1
    s.freq.freq0 = d.freq0
    s.freqStatus = ''
    showToast(`Frequency set: ${d.freq2} ${d.freq1} ${d.freq0}`)
  } catch (e) { s.freqStatus = ''; showToast(`Failed: ${e.message}`, true) }
}

// ── Packet dump ──────────────────────────────────────────────────────────────
export async function startDump() {
  try {
    await api('POST', '/elero/api/dump/start')
    s.dumpActive = true
    showToast('Packet dump started')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

export async function stopDump() {
  try {
    await api('POST', '/elero/api/dump/stop')
    s.dumpActive = false
    showToast('Packet dump stopped')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

export async function clearDump() {
  try {
    await api('POST', '/elero/api/packets/clear')
    s.dumpPackets = []
    showToast('Dump cleared')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

export async function downloadDump() {
  try {
    const d = await api('GET', '/elero/api/packets/download')
    const blob = new Blob([JSON.stringify(d)], { type: 'application/json' })
    const url  = URL.createObjectURL(blob)
    const a    = document.createElement('a')
    a.href = url; a.download = `elero_packets_${Date.now()}.json`; a.click()
    URL.revokeObjectURL(url)
  } catch (e) { showToast(`Download failed: ${e.message}`, true) }
}
