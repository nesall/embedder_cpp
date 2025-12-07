<script lang="ts">
  import { onMount } from "svelte";
  import * as icons from "@lucide/svelte";
  import { instances, projectList } from "../store";
  import { helper_getProjectList, helper_importProject, toaster } from "../utils";
  import InstanceInfo from "./misc/InstanceInfo.svelte";
  import type { InstanceItem, ProjectItem } from "../../app";
  import { slide } from "svelte/transition";

  onMount(() => {
    helper_getProjectList().then((v) => projectList.set(v));
  });

  function onInstClick(j: number) {
    $instances[j]._hidden = !$instances[j]._hidden;
    console.log("Toggled instance", j, $instances[j]._hidden);
  }

  function onImport(j: number) {
    helper_importProject($instances[j].project_id, $instances[j].config).then((res) => {
      helper_getProjectList().then((v) => projectList.set(v));
      if (res.status === "success") {
        console.log("Imported instance into projects:", $instances[j]);
        toaster.success({
          title: "Import Successful",
          description: `Instance ${$instances[j].project_id} imported into Projects.`,
        });
      } else {
        console.log("Failed to import instance:", res.message);
      }
    });
  }

  function isInstInProjectList(inst: InstanceItem, pv: ProjectItem[]) {
    return pv?.some((p) => p.jsonData.source.project_id === inst.project_id);
  }
</script>

<div class="h-full p-4 overflow-auto">
  <form class="w-full">
    <fieldset class="space-y-4">
      <div class="rounded-md shadow p-4 flex flex-col gap-4">
        <div class="text-left font-bold text-lg">List of currently running active instances ({$instances.length})</div>
        <div class="flex flex-col space-y-2">
          {#each $instances as inst, j}
            <div class="border rounded">
              <button type="button" class="btn w-full text-left p-2 flex items-center" onclick={() => onInstClick(j)}>
                <div class="flex items-center space-x-2">
                  <div><span>Instance:&nbsp;</span><span class="font-bold">{inst.project_id}</span></div>
                  {#if !isInstInProjectList(inst, $projectList)}
                    <span class="italic text-error-500 text-sm">(not in projects)</span>
                  {/if}
                </div>
                <span class="ml-auto">
                  {#if inst._hidden}
                    <icons.ChevronDown />
                  {:else}
                    <icons.ChevronUp />
                  {/if}
                </span>
              </button>
              {#if !inst._hidden}
                <div transition:slide>
                  {#if !isInstInProjectList(inst, $projectList)}
                    <div class="w-full flex items-center space-x-4 px-4">
                      <button
                        type="button"
                        class="btn btn-sm preset-filled-secondary-500 flex items-center"
                        onclick={() => onImport(j)}
                      >
                        <icons.Import size={16} />
                        Click to improt into projects
                      </button>
                      <span class="text-surface-500">
                        Import into projects to be able to manage/edit project settings
                      </span>
                    </div>
                  {/if}
                  <InstanceInfo instance={inst} />
                </div>
              {/if}
            </div>
          {/each}
        </div>
      </div>
    </fieldset>
  </form>
</div>
