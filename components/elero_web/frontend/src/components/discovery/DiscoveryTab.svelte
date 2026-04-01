<script>
  import { Badge } from 'flowbite-svelte'
  import { scanning, startScan, stopScan, startAdopt, showYamlBlind, downloadYaml, getDiscoveredNew, getDiscoveredKnown, allDiscovered } from '../../lib/stores.svelte.js'
  import { stateLabel, stateColor, relTime } from '../../lib/utils.js'
  import DiscoveredCard from './DiscoveredCard.svelte'
</script>

<!-- Scan control card -->
<div class="bg-white border border-gray-200 rounded-lg shadow-sm dark:bg-gray-800 dark:border-gray-700 mb-4">
  <div class="flex items-center justify-between p-4 border-b border-gray-200 dark:border-gray-700">
    <span class="text-sm font-medium text-gray-900 dark:text-white">RF Scan</span>
    <div class="flex items-center gap-2">
      <span class="relative flex h-2.5 w-2.5">
        {#if scanning}
          <span class="animate-ping absolute inline-flex h-full w-full rounded-full bg-green-400 opacity-75"></span>
          <span class="relative inline-flex rounded-full h-2.5 w-2.5 bg-green-500"></span>
        {:else}
          <span class="relative inline-flex rounded-full h-2.5 w-2.5 bg-gray-300 dark:bg-gray-600"></span>
        {/if}
      </span>
      <span class="text-xs text-gray-500 dark:text-gray-400">{scanning ? 'Scanning...' : 'Idle'}</span>
    </div>
  </div>
  <div class="p-4">
    <div class="flex flex-wrap gap-2">
      <button onclick={startScan} disabled={scanning}
        class="px-4 py-2 text-xs font-medium text-white bg-primary-600 rounded-lg hover:bg-primary-700 disabled:opacity-50 disabled:cursor-not-allowed dark:bg-primary-500 dark:hover:bg-primary-600 transition-colors">
        Start Scan
      </button>
      <button onclick={stopScan} disabled={!scanning}
        class="px-4 py-2 text-xs font-medium text-white bg-red-600 rounded-lg hover:bg-red-700 disabled:opacity-50 disabled:cursor-not-allowed dark:bg-red-500 dark:hover:bg-red-600 transition-colors">
        Stop Scan
      </button>
    </div>
    <p class="text-xs text-gray-500 dark:text-gray-400 mt-3">Operate your Elero remote during the scan to capture blinds.</p>
  </div>
</div>

<!-- New / unconfigured -->
{#if getDiscoveredNew().length === 0 && !scanning}
  <div class="text-center py-12 text-gray-500 dark:text-gray-400">
    <svg class="w-10 h-10 mx-auto mb-3 text-gray-300 dark:text-gray-600" fill="none" stroke="currentColor" viewBox="0 0 24 24" stroke-width="1.5">
      <path d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"/>
    </svg>
    <p class="text-sm">No unconfigured blinds found.</p>
    <p class="text-xs mt-1">Start a scan and press your remote.</p>
  </div>
{:else}
  <div class="grid grid-cols-1 md:grid-cols-2 gap-3">
    {#each getDiscoveredNew() as blind (blind.blind_address)}
      <DiscoveredCard {blind} isKnown={false} />
    {/each}
  </div>
{/if}

<!-- Already known -->
{#if getDiscoveredKnown().length > 0}
  <h3 class="text-xs font-medium text-gray-500 dark:text-gray-400 mt-6 mb-3 uppercase tracking-wider">Already Configured</h3>
  <div class="grid grid-cols-1 md:grid-cols-2 gap-3 opacity-70">
    {#each getDiscoveredKnown() as blind (blind.blind_address)}
      <DiscoveredCard {blind} isKnown={true} />
    {/each}
  </div>
{/if}

<!-- Download YAML -->
{#if allDiscovered.length > 0}
  <div class="text-center mt-6">
    <button onclick={downloadYaml}
      class="px-4 py-2 text-xs font-medium text-gray-700 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 dark:bg-gray-800 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-700">
      Download all YAML
    </button>
  </div>
{/if}
