<script>
  import { s, applyPreset, setFrequency } from '../../lib/stores.svelte.js'
</script>

<div class="bg-white border border-gray-200 rounded-lg shadow-sm dark:bg-gray-800 dark:border-gray-700">
  <div class="p-4 border-b border-gray-200 dark:border-gray-700">
    <span class="text-sm font-medium text-gray-900 dark:text-white">CC1101 Frequency</span>
  </div>
  <div class="p-4 space-y-4">
    <div>
      <select onchange={(e) => applyPreset(e.target.value)}
        class="w-full bg-white border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
        <option value="">-- Preset --</option>
        <option value="21,71,7a">868.35 MHz (Standard Elero)</option>
        <option value="21,65,c0">868.95 MHz (Alternative)</option>
        <option value="10,A7,62">433.92 MHz</option>
      </select>
    </div>
    <div class="flex flex-wrap gap-3">
      <div class="flex flex-col gap-1">
        <label class="text-[11px] text-gray-500 dark:text-gray-400">freq2</label>
        <input bind:value={s.freq.freq2} maxlength="4"
          class="w-24 font-mono bg-white border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
      </div>
      <div class="flex flex-col gap-1">
        <label class="text-[11px] text-gray-500 dark:text-gray-400">freq1</label>
        <input bind:value={s.freq.freq1} maxlength="4"
          class="w-24 font-mono bg-white border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
      </div>
      <div class="flex flex-col gap-1">
        <label class="text-[11px] text-gray-500 dark:text-gray-400">freq0</label>
        <input bind:value={s.freq.freq0} maxlength="4"
          class="w-24 font-mono bg-white border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
      </div>
    </div>
    <div class="flex items-center gap-3">
      <button onclick={setFrequency}
        class="px-4 py-2 text-xs font-medium text-white bg-primary-600 rounded-lg hover:bg-primary-700 dark:bg-primary-500 dark:hover:bg-primary-600 transition-colors">
        Apply
      </button>
      {#if s.freqStatus}
        <span class="text-xs text-gray-500 dark:text-gray-400">{s.freqStatus}</span>
      {/if}
    </div>
    <p class="text-xs text-gray-500 dark:text-gray-400">Hex values (e.g. 0x7a). Changes take effect immediately without reboot.</p>
  </div>
</div>
