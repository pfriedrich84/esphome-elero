<script>
  import { Badge } from 'flowbite-svelte'
  import { s, lightCmd } from '../../lib/stores.svelte.js'
  import { stateLabel, stateColor, relTime, rssiIcon } from '../../lib/utils.js'

  let { light } = $props()

  let rssi = $derived(rssiIcon(light.rssi))
  let brightnessPercent = $derived(light.brightness !== undefined ? Math.round(light.brightness * 100) : null)
</script>

<div class="bg-white border border-gray-200 rounded-lg shadow-sm dark:bg-gray-800 dark:border-gray-700">
  <!-- Header -->
  <div class="p-4 pb-3">
    <div class="flex items-start justify-between">
      <div class="min-w-0">
        <div class="flex items-center gap-2">
          <svg class="w-4 h-4 text-yellow-400" fill="currentColor" viewBox="0 0 20 20">
            <path d="M10 2a1 1 0 011 1v1a1 1 0 11-2 0V3a1 1 0 011-1zm4 8a4 4 0 11-8 0 4 4 0 018 0zm-.464 4.95l.707.707a1 1 0 001.414-1.414l-.707-.707a1 1 0 00-1.414 1.414zm2.12-10.607a1 1 0 010 1.414l-.706.707a1 1 0 11-1.414-1.414l.707-.707a1 1 0 011.414 0zM17 11a1 1 0 100-2h-1a1 1 0 100 2h1zm-7 4a1 1 0 011 1v1a1 1 0 11-2 0v-1a1 1 0 011-1zM5.05 6.464A1 1 0 106.465 5.05l-.708-.707a1 1 0 00-1.414 1.414l.707.707zm1.414 8.486l-.707.707a1 1 0 01-1.414-1.414l.707-.707a1 1 0 011.414 1.414zM4 11a1 1 0 100-2H3a1 1 0 000 2h1z"/>
          </svg>
          <h3 class="text-sm font-semibold text-gray-900 dark:text-white truncate">{light.name}</h3>
        </div>
        <div class="flex flex-wrap items-center gap-x-2 gap-y-1 mt-1 text-xs text-gray-500 dark:text-gray-400">
          <span class="font-mono">{light.blind_address}</span>
          {#if light.channel}
            <span>CH {light.channel}</span>
          {/if}
          {#if light.rssi}
            <span title="{rssi.label}">{rssi.label}</span>
          {/if}
        </div>
      </div>
      <div class="flex flex-col items-end gap-1">
        <Badge color={stateColor(light.last_state)} class="text-xs">{stateLabel(light.last_state)}</Badge>
        {#if light.adopted}
          <span class="text-[10px] px-1.5 py-0.5 border border-primary-300 text-primary-600 dark:border-primary-600 dark:text-primary-400 rounded">adopted</span>
        {/if}
      </div>
    </div>

    <!-- Brightness bar (dimmable lights only) -->
    {#if light.dim_duration_ms > 0 && brightnessPercent !== null}
      <div class="mt-3">
        <div class="flex justify-between items-center mb-1">
          <span class="text-xs text-gray-500 dark:text-gray-400">Brightness</span>
          <span class="text-xs font-medium text-gray-700 dark:text-gray-300">{brightnessPercent}%</span>
        </div>
        <div class="w-full bg-gray-200 rounded-full h-2 dark:bg-gray-700">
          <div class="bg-yellow-400 h-2 rounded-full transition-all duration-500" style="width: {brightnessPercent}%"></div>
        </div>
      </div>
    {/if}

    <!-- Last seen -->
    <p class="text-[11px] text-gray-400 dark:text-gray-500 mt-2">Last seen: {relTime(light.last_seen_ms, s.uptimeMs)}</p>
  </div>

  <!-- Control buttons -->
  <div class="flex items-center gap-2 px-4 pb-4">
    <button onclick={() => lightCmd(light, 'on')}
      class="flex-1 inline-flex items-center justify-center px-3 py-2 text-xs font-medium text-green-700 bg-green-50 border border-green-200 rounded-lg hover:bg-green-100 dark:bg-green-900/30 dark:text-green-400 dark:border-green-800 dark:hover:bg-green-900/50 transition-colors">
      On
    </button>
    {#if light.dim_duration_ms > 0}
      <button onclick={() => lightCmd(light, 'stop')}
        class="flex-1 inline-flex items-center justify-center px-3 py-2 text-xs font-medium text-gray-700 bg-gray-50 border border-gray-200 rounded-lg hover:bg-gray-100 dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600 transition-colors">
        Stop
      </button>
    {/if}
    <button onclick={() => lightCmd(light, 'off')}
      class="flex-1 inline-flex items-center justify-center px-3 py-2 text-xs font-medium text-red-700 bg-red-50 border border-red-200 rounded-lg hover:bg-red-100 dark:bg-red-900/30 dark:text-red-400 dark:border-red-800 dark:hover:bg-red-900/50 transition-colors">
      Off
    </button>
  </div>
</div>
