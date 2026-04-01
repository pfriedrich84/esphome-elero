<script>
  import { logCapture, logLevel, logAutoScroll, startCapture, stopCapture, clearLog, getFilteredLog, covers, lights } from '../../lib/stores.svelte.js'
  import { fmtTs, linkAddrs } from '../../lib/utils.js'
  import { tick } from 'svelte'

  let logBox

  // Auto-scroll when new entries arrive
  let filteredLog = $derived(getFilteredLog())
  let prevLen = 0

  $effect(() => {
    if (filteredLog.length > prevLen && logAutoScroll && logBox) {
      prevLen = filteredLog.length
      tick().then(() => {
        if (logBox) logBox.scrollTop = logBox.scrollHeight
      })
    }
  })

  const levelColors = {
    error:   'text-red-400',
    warn:    'text-orange-400',
    info:    'text-green-400',
    debug:   'text-blue-400',
    verbose: 'text-gray-500',
  }
</script>

<div class="bg-white border border-gray-200 rounded-lg shadow-sm dark:bg-gray-800 dark:border-gray-700">
  <!-- Controls -->
  <div class="flex flex-wrap items-center justify-between gap-3 p-4 border-b border-gray-200 dark:border-gray-700">
    <div class="flex flex-wrap items-center gap-2">
      <!-- Status dot -->
      <span class="relative flex h-2.5 w-2.5">
        {#if logCapture}
          <span class="animate-ping absolute inline-flex h-full w-full rounded-full bg-green-400 opacity-75"></span>
          <span class="relative inline-flex rounded-full h-2.5 w-2.5 bg-green-500"></span>
        {:else}
          <span class="relative inline-flex rounded-full h-2.5 w-2.5 bg-gray-300 dark:bg-gray-600"></span>
        {/if}
      </span>
      <button onclick={startCapture} disabled={logCapture}
        class="px-3 py-1.5 text-xs font-medium text-white bg-primary-600 rounded-lg hover:bg-primary-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors">
        Start
      </button>
      <button onclick={stopCapture} disabled={!logCapture}
        class="px-3 py-1.5 text-xs font-medium text-white bg-red-600 rounded-lg hover:bg-red-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors">
        Stop
      </button>
      <span class="text-xs text-gray-500 dark:text-gray-400">Level:</span>
      <select bind:value={logLevel}
        class="bg-white border border-gray-300 text-gray-900 text-xs rounded-lg p-1.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
        <option value={5}>VERBOSE</option>
        <option value={4}>DEBUG</option>
        <option value={3}>INFO</option>
        <option value={2}>WARN</option>
        <option value={1}>ERROR</option>
      </select>
    </div>
    <div class="flex items-center gap-3">
      <label class="flex items-center gap-1.5 text-xs text-gray-600 dark:text-gray-400 cursor-pointer">
        <input type="checkbox" bind:checked={logAutoScroll}
          class="w-3.5 h-3.5 text-primary-600 bg-gray-100 border-gray-300 rounded focus:ring-primary-500 dark:bg-gray-700 dark:border-gray-600">
        Auto-scroll
      </label>
      <button onclick={clearLog}
        class="px-3 py-1.5 text-xs font-medium text-gray-700 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600">
        Clear
      </button>
    </div>
  </div>

  <!-- Log viewer (dark terminal) -->
  <div bind:this={logBox}
    class="bg-[#1e1e2e] text-gray-200 font-mono text-[11px] leading-relaxed p-3 max-h-[420px] overflow-y-auto rounded-b-lg">
    {#if filteredLog.length === 0}
      <div class="text-center py-8 text-gray-500">
        No log entries. {!logCapture ? 'Start capture first.' : ''}
      </div>
    {:else}
      {#each filteredLog as entry (entry.t + '_' + entry.idx)}
        <div class="flex gap-2 py-px hover:bg-white/5">
          <span class="text-gray-500 min-w-[52px] shrink-0">{fmtTs(entry.t)}</span>
          <span class="{levelColors[entry.level_str] || 'text-gray-500'} min-w-[44px] shrink-0 font-bold">{entry.level_str.toUpperCase().padEnd(5)}</span>
          <span class="text-cyan-400 min-w-[80px] shrink-0">{entry.tag}</span>
          <span class="break-all">{@html linkAddrs(entry.msg, covers, lights)}</span>
        </div>
      {/each}
    {/if}
  </div>
</div>
