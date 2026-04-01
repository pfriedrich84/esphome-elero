<script>
  import { saveSettings, settingsOpen } from '../../lib/stores.svelte.js'

  let { cover } = $props()
</script>

<div class="border-t border-gray-200 dark:border-gray-700 px-4 py-3 bg-gray-50 dark:bg-gray-800/50 rounded-b-lg">
  <div class="space-y-3">
    <div class="flex items-center gap-3">
      <label class="text-xs text-gray-500 dark:text-gray-400 w-36 shrink-0">Open duration (ms)</label>
      <input type="number" min="0" bind:value={cover._edit.open_duration_ms}
        class="bg-white border border-gray-300 text-gray-900 text-xs rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2 w-32 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
    </div>
    <div class="flex items-center gap-3">
      <label class="text-xs text-gray-500 dark:text-gray-400 w-36 shrink-0">Close duration (ms)</label>
      <input type="number" min="0" bind:value={cover._edit.close_duration_ms}
        class="bg-white border border-gray-300 text-gray-900 text-xs rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2 w-32 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
    </div>
    <div class="flex items-center gap-3">
      <label class="text-xs text-gray-500 dark:text-gray-400 w-36 shrink-0">Poll interval (ms)</label>
      <input type="number" min="0" bind:value={cover._edit.poll_interval_ms}
        class="bg-white border border-gray-300 text-gray-900 text-xs rounded-lg focus:ring-primary-500 focus:border-primary-500 p-2 w-32 dark:bg-gray-700 dark:border-gray-600 dark:text-white">
    </div>
    <div class="flex justify-end gap-2 pt-1">
      <button onclick={() => { settingsOpen = null }}
        class="px-3 py-1.5 text-xs text-gray-700 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600">
        Cancel
      </button>
      <button onclick={() => saveSettings(cover)}
        class="px-3 py-1.5 text-xs text-white bg-primary-600 rounded-lg hover:bg-primary-700 dark:bg-primary-500 dark:hover:bg-primary-600">
        Save
      </button>
    </div>
  </div>
</div>
