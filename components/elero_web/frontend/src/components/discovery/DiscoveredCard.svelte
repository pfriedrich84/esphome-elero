<script>
  import { Badge } from 'flowbite-svelte'
  import { startAdopt, showYamlBlind } from '../../lib/stores.svelte.js'
  import { stateLabel, stateColor, relTime } from '../../lib/utils.js'

  let { blind, isKnown = false } = $props()
</script>

<div class="bg-white border border-gray-200 rounded-lg shadow-sm p-4 dark:bg-gray-800 dark:border-gray-700">
  <div class="flex items-center justify-between mb-2">
    <span class="font-mono text-sm text-gray-900 dark:text-white">{blind.blind_address}</span>
    {#if isKnown}
      <span class="text-xs font-medium text-green-600 dark:text-green-400">
        {blind.already_adopted ? 'Adopted' : 'Configured'}
      </span>
    {:else}
      <Badge color={stateColor(blind.last_state)} class="text-xs">{stateLabel(blind.last_state)}</Badge>
    {/if}
  </div>
  <div class="flex flex-wrap gap-x-3 gap-y-1 text-xs text-gray-500 dark:text-gray-400">
    <span>CH {blind.channel}</span>
    {#if !isKnown}
      <span>Remote <span class="font-mono">{blind.remote_address}</span></span>
    {/if}
    <span>{blind.rssi.toFixed(1)} dBm</span>
    <span>{blind.times_seen}x seen</span>
    <span>{relTime(blind.last_seen_ms)}</span>
  </div>
  {#if !isKnown}
    <div class="flex gap-2 mt-3">
      <button onclick={() => showYamlBlind(blind)}
        class="px-3 py-1.5 text-xs font-medium text-gray-700 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600 transition-colors">
        YAML...
      </button>
      <button onclick={() => startAdopt(blind)}
        class="px-3 py-1.5 text-xs font-medium text-white bg-primary-600 rounded-lg hover:bg-primary-700 dark:bg-primary-500 dark:hover:bg-primary-600 transition-colors">
        Adopt
      </button>
    </div>
  {/if}
</div>
