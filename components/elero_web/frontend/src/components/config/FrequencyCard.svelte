<script>
  import { s, applyPreset, setFrequency } from '../../lib/stores.svelte.js'

  const presets = [
    { value: '21,71,7a', label: '868.35 MHz (Standard Elero)' },
    { value: '21,65,c0', label: '868.95 MHz (Alternative)' },
    { value: '10,A7,62', label: '433.92 MHz' },
  ]

  let currentPreset = $derived(
    presets.find(p => {
      const [f2, f1, f0] = p.value.split(',')
      return s.freq.freq2 === '0x' + f2 && s.freq.freq1 === '0x' + f1 && s.freq.freq0 === '0x' + f0
    })?.value || ''
  )
</script>

<div class="bg-white border border-gray-200 rounded-lg shadow-sm dark:bg-gray-800 dark:border-gray-700">
  <div class="p-4 border-b border-gray-200 dark:border-gray-700">
    <span class="text-sm font-medium text-gray-900 dark:text-white">CC1101 Frequency</span>
  </div>
  <div class="p-4 space-y-4">
    <div>
      <select value={currentPreset} onchange={(e) => applyPreset(e.target.value)}
        class="w-full bg-white border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
        <option value="">-- Custom --</option>
        {#each presets as p}
          <option value={p.value}>{p.label}</option>
        {/each}
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
        class="px-4 py-2 text-sm font-medium text-white bg-blue-700 rounded-lg hover:bg-blue-800 focus:ring-4 focus:ring-blue-300 dark:bg-blue-600 dark:hover:bg-blue-700 dark:focus:ring-blue-800 transition-colors">
        Apply
      </button>
      {#if s.freqStatus}
        <span class="text-xs text-gray-500 dark:text-gray-400">{s.freqStatus}</span>
      {/if}
    </div>
    <p class="text-xs text-gray-500 dark:text-gray-400">Hex values (e.g. 0x7a). Changes take effect immediately without reboot.</p>
  </div>
</div>
