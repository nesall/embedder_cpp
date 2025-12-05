<script lang="ts">
  import * as icons from "@lucide/svelte";
  import { selectedProject } from "../../store";

  const jsonData = $derived($selectedProject?.jsonData);
  const projectTitle = $derived($selectedProject?.jsonData.source.project_title);

  const jsonSettingsStr = $derived(JSON.stringify(jsonData || {}, null, 2));
</script>

{#if jsonData}
  <div class="h-full p-4 overflow-y-auto">
    <div class="rounded-md shadow p-4 flex flex-col gap-4">
      <div class="mb-4 flex items-center justify-between">
        <h2 class="text-xl font-bold flex items-center gap-2">
          <icons.FileBraces size={24} />
          JSON Configuration
        </h2>
        <code class="px-2 rounded text-lg">{projectTitle}</code>
      </div>
      <pre class="pre text-left min-h-40 overflow-x-auto">{jsonSettingsStr}</pre>
    </div>
  </div>
{:else}
  <div>Unable to generate the settings JSON</div>
{/if}
