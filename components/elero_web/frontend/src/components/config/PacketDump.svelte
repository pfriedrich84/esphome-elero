<script>
  import { Badge } from 'flowbite-svelte'
  import { dumpActive, dumpPackets, startDump, stopDump, clearDump, downloadDump } from '../../lib/stores.svelte.js'
</script>

<div class="bg-white border border-gray-200 rounded-lg shadow-sm dark:bg-gray-800 dark:border-gray-700">
  <div class="flex items-center justify-between p-4 border-b border-gray-200 dark:border-gray-700">
    <span class="text-sm font-medium text-gray-900 dark:text-white">Packet Dump</span>
    <div class="flex items-center gap-2">
      <Badge color={dumpActive ? 'green' : 'dark'} class="text-xs">{dumpActive ? 'Active' : 'Idle'}</Badge>
      {#if dumpPackets.length > 0}
        <Badge color="blue" class="text-xs">{dumpPackets.length}</Badge>
      {/if}
    </div>
  </div>
  <div class="p-4 space-y-3">
    <div class="flex flex-wrap gap-2">
      <button onclick={startDump} disabled={dumpActive}
        class="px-3 py-1.5 text-xs font-medium text-white bg-primary-600 rounded-lg hover:bg-primary-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors">
        Start Dump
      </button>
      <button onclick={stopDump} disabled={!dumpActive}
        class="px-3 py-1.5 text-xs font-medium text-white bg-red-600 rounded-lg hover:bg-red-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors">
        Stop
      </button>
      <button onclick={clearDump}
        class="px-3 py-1.5 text-xs font-medium text-gray-700 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600 transition-colors">
        Clear
      </button>
      <button onclick={downloadDump} disabled={dumpPackets.length === 0}
        class="px-3 py-1.5 text-xs font-medium text-gray-700 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 disabled:opacity-50 disabled:cursor-not-allowed dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600 transition-colors">
        Download
      </button>
    </div>
    <p class="text-xs text-gray-500 dark:text-gray-400">Records all received RF packets (max 50). Green = valid, red = rejected.</p>

    {#if dumpPackets.length > 0}
      <div class="overflow-x-auto max-h-[360px] overflow-y-auto rounded-lg border border-gray-200 dark:border-gray-700">
        <table class="w-full text-xs text-left">
          <thead class="text-xs text-gray-700 uppercase bg-gray-50 dark:bg-gray-700 dark:text-gray-400 sticky top-0">
            <tr>
              <th class="px-3 py-2">Time (ms)</th>
              <th class="px-3 py-2">Len</th>
              <th class="px-3 py-2">Status</th>
              <th class="px-3 py-2">Reason</th>
              <th class="px-3 py-2">Hex</th>
            </tr>
          </thead>
          <tbody>
            {#each [...dumpPackets].reverse() as pkt (pkt.t)}
              <tr class="{pkt.valid ? 'bg-green-50 dark:bg-green-900/20' : 'bg-red-50 dark:bg-red-900/20'} border-b dark:border-gray-700">
                <td class="px-3 py-1.5">{pkt.t}</td>
                <td class="px-3 py-1.5">{pkt.len}</td>
                <td class="px-3 py-1.5">
                  <span class="font-semibold {pkt.valid ? 'text-green-600 dark:text-green-400' : 'text-red-600 dark:text-red-400'}">
                    {pkt.valid ? 'OK' : 'ERR'}
                  </span>
                </td>
                <td class="px-3 py-1.5">{pkt.reason || ''}</td>
                <td class="px-3 py-1.5 font-mono text-[10px] break-all">{pkt.hex}</td>
              </tr>
            {/each}
          </tbody>
        </table>
      </div>
    {/if}
  </div>
</div>
