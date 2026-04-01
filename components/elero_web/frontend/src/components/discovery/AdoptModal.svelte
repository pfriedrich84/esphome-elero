<script>
  import { s, confirmAdopt } from '../../lib/stores.svelte.js'

  function close() { s.adoptTarget = null }
</script>

{#if s.adoptTarget}
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="fixed inset-0 z-50 flex items-center justify-center bg-gray-900/50 dark:bg-gray-900/80"
       onclick={(e) => { if (e.target === e.currentTarget) close() }}
       onkeydown={(e) => { if (e.key === 'Escape') close() }}>
    <div class="relative w-full max-w-md mx-4 bg-white rounded-lg shadow-xl dark:bg-gray-800">
      <div class="flex items-center justify-between p-4 border-b dark:border-gray-700">
        <h3 class="text-sm font-semibold text-gray-900 dark:text-white">
          Adopt blind <span class="font-mono">{s.adoptTarget.blind_address}</span>
        </h3>
        <button onclick={close} class="text-gray-400 hover:text-gray-900 dark:hover:text-white text-xl leading-none">&times;</button>
      </div>
      <div class="p-4 space-y-4">
        <div>
          <label class="block text-xs font-medium text-gray-700 dark:text-gray-300 mb-1">Friendly name</label>
          <input type="text" bind:value={s.adoptName} placeholder="e.g. Balcony"
            class="w-full bg-white border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white dark:placeholder-gray-400">
        </div>
        <div>
          <label class="block text-xs font-medium text-gray-700 dark:text-gray-300 mb-1">Device type</label>
          <select bind:value={s.adoptType}
            class="w-full bg-white border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
            <option value="cover">Cover (blind/shutter)</option>
            <option value="light">Light (dimmer)</option>
          </select>
        </div>
        <p class="text-xs text-gray-500 dark:text-gray-400">
          This device will appear in the Devices tab. For permanent HA integration, add the YAML to your config and reflash.
        </p>
      </div>
      <div class="flex justify-end gap-2 p-4 border-t dark:border-gray-700">
        <button onclick={close}
          class="px-4 py-2 text-sm text-gray-700 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600">
          Cancel
        </button>
        <button onclick={confirmAdopt}
          class="px-4 py-2 text-sm text-white bg-primary-600 rounded-lg hover:bg-primary-700 dark:bg-primary-500 dark:hover:bg-primary-600">
          Adopt
        </button>
      </div>
    </div>
  </div>
{/if}
