<script>
  import { s, copyYaml } from '../../lib/stores.svelte.js'

  function close() { s.yamlContent = null }
</script>

{#if s.yamlContent}
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="fixed inset-0 z-50 flex items-center justify-center bg-gray-900/50 dark:bg-gray-900/80"
       onclick={(e) => { if (e.target === e.currentTarget) close() }}
       onkeydown={(e) => { if (e.key === 'Escape') close() }}>
    <div class="relative w-full max-w-lg mx-4 bg-white rounded-lg shadow-xl dark:bg-gray-800">
      <div class="flex items-center justify-between p-4 border-b dark:border-gray-700">
        <h3 class="text-sm font-semibold text-gray-900 dark:text-white">YAML Snippet</h3>
        <button onclick={close} class="text-gray-400 hover:text-gray-900 dark:hover:text-white text-xl leading-none">&times;</button>
      </div>
      <div class="p-4">
        <p class="text-xs text-gray-500 dark:text-gray-400 mb-3">Add this to your ESPHome configuration for permanent HA integration:</p>
        <pre class="bg-[#263238] text-gray-200 font-mono text-xs p-4 rounded-lg overflow-x-auto max-h-[360px] whitespace-pre">{s.yamlContent}</pre>
      </div>
      <div class="flex justify-end gap-2 p-4 border-t dark:border-gray-700">
        <button onclick={copyYaml}
          class="px-4 py-2 text-sm text-gray-700 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600">
          Copy
        </button>
        <button onclick={close}
          class="px-4 py-2 text-sm text-white bg-primary-600 rounded-lg hover:bg-primary-700 dark:bg-primary-500 dark:hover:bg-primary-600">
          Close
        </button>
      </div>
    </div>
  </div>
{/if}
