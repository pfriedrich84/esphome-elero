<script>
  import { deviceName, uptimeMs, freq, covers, lights } from '../../lib/stores.svelte.js'
  import { uptimeFmt } from '../../lib/utils.js'
</script>

<div class="bg-white border border-gray-200 rounded-lg shadow-sm dark:bg-gray-800 dark:border-gray-700">
  <div class="p-4 border-b border-gray-200 dark:border-gray-700">
    <span class="text-sm font-medium text-gray-900 dark:text-white">Device Info</span>
  </div>
  <div class="p-4">
    <dl class="grid grid-cols-[130px_1fr] gap-x-4 gap-y-2 text-sm">
      <dt class="text-gray-500 dark:text-gray-400 font-medium">Device</dt>
      <dd class="text-gray-900 dark:text-white">{deviceName || '-'}</dd>

      <dt class="text-gray-500 dark:text-gray-400 font-medium">Uptime</dt>
      <dd class="text-gray-900 dark:text-white">{uptimeFmt(uptimeMs)}</dd>

      <dt class="text-gray-500 dark:text-gray-400 font-medium">Frequency</dt>
      <dd class="font-mono text-gray-900 dark:text-white">{freq.freq2} / {freq.freq1} / {freq.freq0}</dd>

      <dt class="text-gray-500 dark:text-gray-400 font-medium">Covers</dt>
      <dd class="text-gray-900 dark:text-white">
        {covers.filter(c => !c.adopted).length} configured, {covers.filter(c => c.adopted).length} adopted
      </dd>

      <dt class="text-gray-500 dark:text-gray-400 font-medium">Lights</dt>
      <dd class="text-gray-900 dark:text-white">
        {lights.filter(l => !l.adopted).length} configured, {lights.filter(l => l.adopted).length} adopted
      </dd>
    </dl>
  </div>
</div>
