<script>
  import { s } from '../../lib/stores.svelte.js'
  import CoverCard from './CoverCard.svelte'
  import LightCard from './LightCard.svelte'
</script>

{#if s.covers.length === 0 && s.lights.length === 0}
  <div class="flex flex-col items-center justify-center py-16 text-gray-500 dark:text-gray-400">
    <svg class="w-12 h-12 mb-4 text-gray-300 dark:text-gray-600" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
      <rect x="3" y="3" width="18" height="18" rx="2"/><line x1="3" y1="9" x2="21" y2="9"/><line x1="3" y1="15" x2="21" y2="15"/>
    </svg>
    <p class="text-sm">No devices configured.</p>
    <p class="text-xs mt-1">Add covers or lights in your ESPHome YAML.</p>
  </div>
{:else}
  <div class="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-4">
    {#each s.covers as cover (cover.blind_address)}
      <CoverCard {cover} />
    {/each}
    {#each s.lights as light (light.blind_address)}
      <LightCard {light} />
    {/each}
  </div>
{/if}
