export const STATE_LABELS = {
  top:               'Top',
  bottom:            'Bottom',
  intermediate:      'Intermediate',
  tilt:              'Tilt',
  blocking:          'Blocking',
  overheated:        'Overheated',
  timeout:           'Timeout',
  start_moving_up:   'Starting Up',
  start_moving_down: 'Starting Down',
  moving_up:         'Moving Up',
  moving_down:       'Moving Down',
  stopped:           'Stopped',
  top_tilt:          'Top (Tilt)',
  bottom_tilt:       'Bottom (Tilt)',
  unknown:           'Unknown',
  on:                'On',
  off:               'Off',
}

export function stateLabel(s) {
  return STATE_LABELS[s] || s || 'Unknown'
}

export function stateColor(s) {
  if (s === 'top') return 'green'
  if (s === 'bottom') return 'red'
  if (s?.includes('moving') || s?.startsWith('start_moving')) return 'yellow'
  if (s === 'intermediate' || s === 'stopped') return 'blue'
  if (s === 'blocking' || s === 'overheated' || s === 'timeout') return 'red'
  if (s?.includes('tilt')) return 'purple'
  if (s === 'on') return 'green'
  if (s === 'off') return 'dark'
  return 'dark'
}

export function relTime(ms) {
  if (!ms) return 'never'
  const diff = Math.floor((Date.now() - ms) / 1000)
  if (diff < 5)    return 'just now'
  if (diff < 60)   return `${diff}s ago`
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`
  return `${Math.floor(diff / 3600)}h ago`
}

export function fmtTs(ms) {
  const s = Math.floor(ms / 1000)
  const h = Math.floor(s / 3600) % 24
  const m = Math.floor(s / 60) % 60
  const sec = s % 60
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(sec).padStart(2, '0')}`
}

export function uptimeFmt(ms) {
  if (!ms) return ''
  const s = Math.floor(ms / 1000)
  const h = Math.floor(s / 3600)
  const m = Math.floor((s % 3600) / 60)
  const sec = s % 60
  if (h > 0) return `${h}h ${m}m ${sec}s`
  if (m > 0) return `${m}m ${sec}s`
  return `${sec}s`
}

export function rssiIcon(rssi) {
  if (rssi >= -65) return { bars: 3, label: rssi.toFixed(1) + ' dBm' }
  if (rssi >= -80) return { bars: 2, label: rssi.toFixed(1) + ' dBm' }
  return { bars: 1, label: rssi.toFixed(1) + ' dBm' }
}

/** Escape HTML entities */
export function escHtml(s) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}

/** Replace 0xABCDEF hex addresses with name annotations in log messages */
export function linkAddrs(msg, covers, lights) {
  if (!msg) return ''
  const addrMap = {}
  for (const c of covers) addrMap[c.blind_address] = c.name
  for (const l of lights) addrMap[l.blind_address] = l.name
  const safe = escHtml(msg)
  return safe.replace(/0x[0-9a-fA-F]{6}/g, m => {
    const name = addrMap[m.toLowerCase()] || addrMap[m]
    if (!name) return m
    const escName = escHtml(name).replace(/"/g, '&quot;')
    return `${m}<span class="font-semibold text-green-400">(${escName})</span>`
  })
}
