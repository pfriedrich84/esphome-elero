import { api } from './api.js'

// ── Navigation ───────────────────────────────────────────────────────────────
export let tab = $state('devices')

// ── Device info ──────────────────────────────────────────────────────────────
export let deviceName = $state('')
export let uptimeMs = $state(0)

// ── Devices ──────────────────────────────────────────────────────────────────
export let covers = $state([])
export let lights = $state([])
export let settingsOpen = $state(null)

// ── Discovery ────────────────────────────────────────────────────────────────
export let scanning = $state(false)
export let allDiscovered = $state([])

export function getDiscoveredNew() {
  return allDiscovered.filter(b => !b.already_configured && !b.already_adopted)
}
export function getDiscoveredKnown() {
  return allDiscovered.filter(b => b.already_configured || b.already_adopted)
}

// ── Adopt modal ──────────────────────────────────────────────────────────────
export let adoptTarget = $state(null)
export let adoptName = $state('')
export let adoptType = $state('cover')

// ── YAML modal ───────────────────────────────────────────────────────────────
export let yamlContent = $state(null)

// ── Log ──────────────────────────────────────────────────────────────────────
export let logCapture = $state(false)
export let logLevel = $state(3)
export let logAutoScroll = $state(true)
export let logEntries = $state([])
export let logLastTs = $state(0)

export function getFilteredLog() {
  return logEntries.filter(e => e.level <= logLevel)
}

// ── Config ───────────────────────────────────────────────────────────────────
export let freq = $state({ freq2: '', freq1: '', freq0: '' })
export let freqStatus = $state('')
export let dumpActive = $state(false)
export let dumpPackets = $state([])

// ── Toast ────────────────────────────────────────────────────────────────────
export let toast = $state({ show: false, error: false, msg: '' })
let toastTimer = null

export function showToast(msg, isError = false) {
  if (toastTimer) clearTimeout(toastTimer)
  toast.show = true
  toast.error = isError
  toast.msg = msg
  toastTimer = setTimeout(() => { toast.show = false }, 3500)
}

// ── Polling ──────────────────────────────────────────────────────────────────
let polling = false
let pollTimer = null

export async function init() {
  await refreshInfo()
  await refreshStatus()
  await loadFrequency()
  schedulePoll()
}

function schedulePoll() {
  pollTimer = setTimeout(async () => {
    if (polling) { schedulePoll(); return }
    polling = true
    try { await refreshStatus() } catch {}
    polling = false
    schedulePoll()
  }, 3000)
}

export async function refreshInfo() {
  try {
    const d = await api('GET', '/elero/api/info')
    deviceName = d.device_name || ''
    uptimeMs = d.uptime_ms || 0
    freq.freq2 = d.freq2 || freq.freq2
    freq.freq1 = d.freq1 || freq.freq1
    freq.freq0 = d.freq0 || freq.freq0
  } catch {}
}

export async function refreshStatus() {
  const params = { tab }
  if (tab === 'log' && logLastTs) params.since = logLastTs
  const d = await api('GET', '/elero/api/status', params)

  covers = (d.covers || []).map(c => ({
    ...c,
    _edit: {
      open_duration_ms:  c.open_duration_ms,
      close_duration_ms: c.close_duration_ms,
      poll_interval_ms:  c.poll_interval_ms,
    }
  }))
  lights = (d.lights || []).map(l => ({
    ...l,
    _edit: { dim_duration_ms: l.dim_duration_ms }
  }))
  uptimeMs += 3000

  if (d.discovered !== undefined) {
    scanning = d.scanning
    allDiscovered = d.discovered || []
  }
  if (d.log_entries !== undefined) {
    logCapture = d.capture_active
    if (d.log_entries.length > 0) {
      const ne = d.log_entries.map((e, i) => ({ ...e, idx: logEntries.length + i }))
      logEntries = [...logEntries, ...ne]
      if (logEntries.length > 500) logEntries = logEntries.slice(logEntries.length - 500)
      logLastTs = ne[ne.length - 1].t
    }
  }
  if (d.packets !== undefined) {
    dumpActive = d.dump_active
    dumpPackets = d.packets || []
  }
}

// ── Cover commands ───────────────────────────────────────────────────────────
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
    settingsOpen = null
  } catch (e) { showToast(`Save failed: ${e.message}`, true) }
}

// ── Discovery ────────────────────────────────────────────────────────────────
export async function startScan() {
  try {
    await api('POST', '/elero/api/scan/start')
    scanning = true
    showToast('Scan started')
  } catch (e) { showToast(`Scan start failed: ${e.message}`, true) }
}

export async function stopScan() {
  try {
    await api('POST', '/elero/api/scan/stop')
    scanning = false
    showToast('Scan stopped')
  } catch (e) { showToast(`Scan stop failed: ${e.message}`, true) }
}

export function startAdopt(b) {
  adoptTarget = b
  adoptName = ''
  adoptType = (b.last_state === 'on' || b.last_state === 'off') ? 'light' : 'cover'
}

export async function confirmAdopt() {
  if (!adoptTarget) return
  try {
    await api('POST', `/elero/api/discovered/${adoptTarget.blind_address}/adopt`,
              { name: adoptName || adoptTarget.blind_address, type: adoptType })
    showToast(`Adopted as "${adoptName || adoptTarget.blind_address}"`)
    adoptTarget = null
    tab = 'devices'
  } catch (e) { showToast(`Adopt failed: ${e.message}`, true) }
}

export function showYamlBlind(b) {
  const isLight = b.last_state === 'on' || b.last_state === 'off'
  if (isLight) {
    yamlContent =
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
    yamlContent =
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
  navigator.clipboard?.writeText(yamlContent)
    .then(() => showToast('Copied!'))
    .catch(() => showToast('Copy failed', true))
}

// ── Log ──────────────────────────────────────────────────────────────────────
export async function startCapture() {
  try {
    await api('POST', '/elero/api/logs/capture/start')
    logCapture = true
    showToast('Log capture started')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

export async function stopCapture() {
  try {
    await api('POST', '/elero/api/logs/capture/stop')
    logCapture = false
    showToast('Log capture stopped')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

export async function clearLog() {
  try {
    await api('POST', '/elero/api/logs/clear')
    logEntries = []
    logLastTs = 0
    showToast('Log cleared')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

// ── Frequency ────────────────────────────────────────────────────────────────
export async function loadFrequency() {
  try {
    const d = await api('GET', '/elero/api/frequency')
    freq.freq2 = d.freq2
    freq.freq1 = d.freq1
    freq.freq0 = d.freq0
  } catch {}
}

export function applyPreset(v) {
  if (!v) return
  const [f2, f1, f0] = v.split(',')
  freq.freq2 = '0x' + f2
  freq.freq1 = '0x' + f1
  freq.freq0 = '0x' + f0
}

export async function setFrequency() {
  freqStatus = 'Applying...'
  try {
    const d = await api('POST', '/elero/api/frequency/set', { freq2: freq.freq2, freq1: freq.freq1, freq0: freq.freq0 })
    freq.freq2 = d.freq2
    freq.freq1 = d.freq1
    freq.freq0 = d.freq0
    freqStatus = ''
    showToast(`Frequency set: ${d.freq2} ${d.freq1} ${d.freq0}`)
  } catch (e) { freqStatus = ''; showToast(`Failed: ${e.message}`, true) }
}

// ── Packet dump ──────────────────────────────────────────────────────────────
export async function startDump() {
  try {
    await api('POST', '/elero/api/dump/start')
    dumpActive = true
    showToast('Packet dump started')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

export async function stopDump() {
  try {
    await api('POST', '/elero/api/dump/stop')
    dumpActive = false
    showToast('Packet dump stopped')
  } catch (e) { showToast(`Failed: ${e.message}`, true) }
}

export async function clearDump() {
  try {
    await api('POST', '/elero/api/packets/clear')
    dumpPackets = []
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
