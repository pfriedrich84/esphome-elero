<script>
  import { onMount } from 'svelte'
  import { DarkMode } from 'flowbite-svelte'
  import { CheckCircleSolid, CloseCircleSolid } from 'flowbite-svelte-icons'
  import { s, init, getDiscoveredNew } from './lib/stores.svelte.js'
  import { uptimeFmt } from './lib/utils.js'
  import DevicesTab from './components/devices/DevicesTab.svelte'
  import DiscoveryTab from './components/discovery/DiscoveryTab.svelte'
  import LogTab from './components/log/LogTab.svelte'
  import ConfigTab from './components/config/ConfigTab.svelte'
  import AdoptModal from './components/discovery/AdoptModal.svelte'
  import YamlModal from './components/config/YamlModal.svelte'

  onMount(() => { init() })
</script>

<!-- Navbar -->
<nav class="bg-white border-b border-gray-200 px-4 py-2.5 dark:bg-gray-800 dark:border-gray-700 fixed w-full z-20 top-0 start-0">
  <div class="flex flex-wrap items-center justify-between max-w-screen-xl mx-auto">
    <div class="flex items-center gap-3">
      <svg class="w-7 h-7 text-primary-600 dark:text-primary-400" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <rect x="3" y="3" width="18" height="18" rx="2"/>
        <line x1="3" y1="9" x2="21" y2="9"/>
        <line x1="3" y1="15" x2="21" y2="15"/>
      </svg>
      <span class="text-lg font-semibold text-gray-900 dark:text-white">
        {s.deviceName || 'Elero Blind Manager'}
      </span>
    </div>
    <div class="flex items-center gap-3">
      <span class="text-xs text-gray-500 dark:text-gray-400 hidden sm:inline">
        {uptimeFmt(s.uptimeMs)}
      </span>
      <DarkMode btnClass="text-gray-500 dark:text-gray-400 hover:bg-gray-100 dark:hover:bg-gray-700 rounded-lg text-sm p-2" />
    </div>
  </div>
</nav>

<!-- Main content -->
<main class="pt-16 max-w-screen-xl mx-auto px-4 pb-8">
  <!-- Tab navigation -->
  <div class="border-b border-gray-200 dark:border-gray-700 mt-4">
    <ul class="flex flex-wrap -mb-px text-sm font-medium text-center">
      <li class="me-2">
        <button onclick={() => s.tab = 'devices'}
          class="inline-flex items-center gap-2 p-4 border-b-2 rounded-t-lg {s.tab === 'devices' ? 'text-primary-600 border-primary-600 dark:text-primary-500 dark:border-primary-500' : 'border-transparent hover:text-gray-600 hover:border-gray-300 dark:hover:text-gray-300 text-gray-500 dark:text-gray-400'}">
          Devices
          {#if s.covers.length + s.lights.length > 0}
            <span class="inline-flex items-center justify-center w-5 h-5 text-xs font-semibold text-primary-800 bg-primary-100 rounded-full dark:bg-primary-900 dark:text-primary-300">
              {s.covers.length + s.lights.length}
            </span>
          {/if}
        </button>
      </li>
      <li class="me-2">
        <button onclick={() => s.tab = 'discovery'}
          class="inline-flex items-center gap-2 p-4 border-b-2 rounded-t-lg {s.tab === 'discovery' ? 'text-primary-600 border-primary-600 dark:text-primary-500 dark:border-primary-500' : 'border-transparent hover:text-gray-600 hover:border-gray-300 dark:hover:text-gray-300 text-gray-500 dark:text-gray-400'}">
          Discovery
          {#if getDiscoveredNew().length > 0}
            <span class="inline-flex items-center justify-center w-5 h-5 text-xs font-semibold text-primary-800 bg-primary-100 rounded-full dark:bg-primary-900 dark:text-primary-300">
              {getDiscoveredNew().length}
            </span>
          {/if}
        </button>
      </li>
      <li class="me-2">
        <button onclick={() => s.tab = 'log'}
          class="inline-flex items-center gap-2 p-4 border-b-2 rounded-t-lg {s.tab === 'log' ? 'text-primary-600 border-primary-600 dark:text-primary-500 dark:border-primary-500' : 'border-transparent hover:text-gray-600 hover:border-gray-300 dark:hover:text-gray-300 text-gray-500 dark:text-gray-400'}">
          Log
          {#if s.logEntries.length > 0}
            <span class="inline-flex items-center justify-center w-5 h-5 text-xs font-semibold text-yellow-800 bg-yellow-100 rounded-full dark:bg-yellow-900 dark:text-yellow-300">
              {s.logEntries.length}
            </span>
          {/if}
        </button>
      </li>
      <li class="me-2">
        <button onclick={() => s.tab = 'config'}
          class="inline-flex items-center gap-2 p-4 border-b-2 rounded-t-lg {s.tab === 'config' ? 'text-primary-600 border-primary-600 dark:text-primary-500 dark:border-primary-500' : 'border-transparent hover:text-gray-600 hover:border-gray-300 dark:hover:text-gray-300 text-gray-500 dark:text-gray-400'}">
          Configuration
        </button>
      </li>
    </ul>
  </div>

  <!-- Tab content -->
  <div class="mt-4">
    {#if s.tab === 'devices'}
      <DevicesTab />
    {:else if s.tab === 'discovery'}
      <DiscoveryTab />
    {:else if s.tab === 'log'}
      <LogTab />
    {:else if s.tab === 'config'}
      <ConfigTab />
    {/if}
  </div>
</main>

<!-- Toast -->
{#if s.toast.show}
  <div class="fixed bottom-5 left-1/2 -translate-x-1/2 z-50">
    <div class="flex items-center gap-3 px-4 py-3 rounded-lg shadow-lg text-sm {s.toast.error ? 'text-red-800 bg-red-50 dark:bg-red-800 dark:text-red-200' : 'text-green-800 bg-green-50 dark:bg-green-800 dark:text-green-200'}">
      {#if s.toast.error}
        <CloseCircleSolid class="w-5 h-5" />
      {:else}
        <CheckCircleSolid class="w-5 h-5" />
      {/if}
      {s.toast.msg}
    </div>
  </div>
{/if}

<AdoptModal />
<YamlModal />
