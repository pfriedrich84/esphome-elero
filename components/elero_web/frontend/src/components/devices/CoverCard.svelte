<script>
  import { Badge } from 'flowbite-svelte'
  import { s, coverCmd, saveSettings } from '../../lib/stores.svelte.js'
  import { stateLabel, stateColor, relTime, rssiIcon } from '../../lib/utils.js'
  import SettingsPanel from './SettingsPanel.svelte'

  let { cover } = $props()

  let rssi = $derived(rssiIcon(cover.rssi))
  let posPercent = $derived(cover.position !== null ? Math.round(cover.position * 100) : null)
  let isExpanded = $derived(settingsOpen === cover.blind_address)

  function toggleSettings() {
    settingsOpen = isExpanded ? null : cover.blind_address
  }
</script>

<div class="bg-white border border-gray-200 rounded-lg shadow-sm dark:bg-gray-800 dark:border-gray-700">
  <!-- Header -->
  <div class="p-4 pb-3">
    <div class="flex items-start justify-between">
      <div class="min-w-0">
        <h3 class="text-sm font-semibold text-gray-900 dark:text-white truncate">{cover.name}</h3>
        <div class="flex flex-wrap items-center gap-x-2 gap-y-1 mt-1 text-xs text-gray-500 dark:text-gray-400">
          <span class="font-mono">{cover.blind_address}</span>
          {#if cover.channel}
            <span>CH {cover.channel}</span>
          {/if}
          {#if cover.rssi}
            <span title="{rssi.label}">
              {'|'.repeat(rssi.bars)}{'|'.repeat(3 - rssi.bars).replace(/\|/g, ' ')} {rssi.label}
            </span>
          {/if}
        </div>
      </div>
      <div class="flex flex-col items-end gap-1">
        <Badge color={stateColor(cover.last_state)} class="text-xs">{stateLabel(cover.last_state)}</Badge>
        {#if cover.adopted}
          <span class="text-[10px] px-1.5 py-0.5 border border-primary-300 text-primary-600 dark:border-primary-600 dark:text-primary-400 rounded">adopted</span>
        {/if}
      </div>
    </div>

    <!-- Position bar -->
    {#if posPercent !== null && cover.open_duration_ms > 0}
      <div class="mt-3">
        <div class="flex justify-between items-center mb-1">
          <span class="text-xs text-gray-500 dark:text-gray-400">Position</span>
          <span class="text-xs font-medium text-gray-700 dark:text-gray-300">{posPercent}%</span>
        </div>
        <div class="w-full bg-gray-200 rounded-full h-2 dark:bg-gray-700">
          <div class="bg-primary-600 h-2 rounded-full transition-all duration-500" style="width: {posPercent}%"></div>
        </div>
      </div>
    {/if}

    <!-- Last seen -->
    <p class="text-[11px] text-gray-400 dark:text-gray-500 mt-2">Last seen: {relTime(cover.last_seen_ms)}</p>
  </div>

  <!-- Control buttons -->
  <div class="flex items-center gap-2 px-4 pb-3">
    <button onclick={() => coverCmd(cover, 'open')}
      class="flex-1 inline-flex items-center justify-center gap-1 px-3 py-2 text-xs font-medium text-green-700 bg-green-50 border border-green-200 rounded-lg hover:bg-green-100 dark:bg-green-900/30 dark:text-green-400 dark:border-green-800 dark:hover:bg-green-900/50 transition-colors">
      &#x25B2; Open
    </button>
    <button onclick={() => coverCmd(cover, 'stop')}
      class="flex-1 inline-flex items-center justify-center gap-1 px-3 py-2 text-xs font-medium text-gray-700 bg-gray-50 border border-gray-200 rounded-lg hover:bg-gray-100 dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600 transition-colors">
      &#x25A0; Stop
    </button>
    <button onclick={() => coverCmd(cover, 'close')}
      class="flex-1 inline-flex items-center justify-center gap-1 px-3 py-2 text-xs font-medium text-red-700 bg-red-50 border border-red-200 rounded-lg hover:bg-red-100 dark:bg-red-900/30 dark:text-red-400 dark:border-red-800 dark:hover:bg-red-900/50 transition-colors">
      &#x25BC; Close
    </button>
    {#if cover.supports_tilt}
      <button onclick={() => coverCmd(cover, 'tilt')}
        class="inline-flex items-center justify-center gap-1 px-3 py-2 text-xs font-medium text-primary-700 bg-primary-50 border border-primary-200 rounded-lg hover:bg-primary-100 dark:bg-primary-900/30 dark:text-primary-400 dark:border-primary-800 dark:hover:bg-primary-900/50 transition-colors">
        Tilt
      </button>
    {/if}
    <button onclick={toggleSettings}
      class="inline-flex items-center justify-center p-2 text-gray-500 hover:bg-gray-100 rounded-lg dark:text-gray-400 dark:hover:bg-gray-700 transition-colors"
      title="Settings">
      &#x2699;
    </button>
  </div>

  <!-- Settings panel -->
  {#if isExpanded}
    <SettingsPanel {cover} />
  {/if}
</div>
