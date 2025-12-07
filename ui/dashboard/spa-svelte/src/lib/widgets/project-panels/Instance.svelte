<script lang="ts">
  import { onMount } from "svelte";
  import * as icons from "@lucide/svelte";
  import { instances, selectedProject } from "../../store";
  import { hasRunningInstance } from "../../utils";
  import InstanceInfo from "../misc/InstanceInfo.svelte";

  const jsonData = $derived($selectedProject?.jsonData);
  const projectTitle = $derived($selectedProject?.jsonData.source.project_title);

  onMount(() => {});

  const running = $derived(jsonData && hasRunningInstance(jsonData.source.project_id, $instances));
  const instance = $derived(jsonData && $instances.find((a) => a.project_id == jsonData.source.project_id));

  function onStart() {

  }

  function onStop() {
    
  }
</script>

{#if jsonData}
  <div class="h-full p-4 overflow-auto">
    <form class="w-full">
      <fieldset class="space-y-4">
        <div class="rounded-md shadow p-4 flex flex-col gap-4">
          <div>
            {#if running}
              <div class="p-4 bg-success-300-700 rounded text-lg flex items-center justify-center space-x-4">
                <icons.CircleCheck class="mr-2" />
                <span>An instance of this project is currently <strong>running</strong>.</span>
                <button
                  type="button"
                  class="btn preset-filled font-bold"
                  title="Stop currently running instance."
                  onclick={onStop}
                >
                  STOP
                </button>
              </div>
              <InstanceInfo {instance} />
            {:else}
              <div class="p-4 bg-error-300-700 rounded text-lg flex items-center justify-center space-x-4">
                <icons.CircleX class="mr-2" />
                <span>No instance of this project is currently <strong>running</strong>.</span>
                <button
                  type="button"
                  class="btn preset-filled font-bold"
                  title="Stop currently running instance."
                  onclick={onStart}
                >
                  START
                </button>
              </div>
              <div class="text-left pt-4 text-lg font-bold">Project: {projectTitle}</div>
            {/if}
          </div>
        </div>
      </fieldset>
    </form>
  </div>
{/if}
