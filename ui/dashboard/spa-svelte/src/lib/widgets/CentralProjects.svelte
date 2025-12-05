<script lang="ts">
  import { onMount } from "svelte";
  import * as icons from "@lucide/svelte";
  import ProjectPanel from "./ProjectPanel.svelte";
  import { test_getProjectList, test_savePojectSettings } from "../utils";
  import type { ProjectItem } from "../../app";
  import { selectedProject } from "../store";

  let projects: ProjectItem[] = $state([]);

  onMount(async () => {
    // Fetch and display central projects here
    projects = await test_getProjectList();
  });

  async function onItemClick(item: ProjectItem) {
    // const settings = await test_getPojectSettings(item.jsonData.source.project_id);
    // if ($selectedProject) {
    //   console.log("Saving selectedJsonSettings");
    //   test_setPojectSettings($selectedProject.jsonData.source.project_id, $selectedJsonSettings);
    // }
    console.log("Switching to project", item.jsonData);
    // selectedJsonSettings.set(settings);
    selectedProject.set(item);
  }
</script>

<div class="h-full">
  <div class="h-full flex">
    <div class="h-full max-w-xs flex flex-col space-y-1">
      <div class="flex items-center gap-2 justify-between">
        <button type="button" class="btn btn-sm preset-filled-primary-500"> Create New Project </button>
        <button type="button" class="btn-icon btn-sm preset-filled-error-500"> <icons.Trash2 size={16} /></button>
      </div>
      <ul class="w-full h-full p-1 shadow overflow-y-auto border border-surface-200-800 rounded min-w-54">
        <div class="bg-surface-100-900 p-2 mb-2">View/Edit Projects</div>
        {#each projects as item (item.jsonData.source.project_id)}
          <li class="hover:bg-surface-200-800 px-2 flex items-center space-x-2 border-b border-surface-200-800">
            <button
              type="button"
              class="btn btn p-1 w-full flex items-center space-x-2 justify-start text-sm
              {item.jsonData.source.project_id === $selectedProject?.jsonData.source.project_id ? 'font-bold' : ''}
              "
              onclick={() => onItemClick(item)}
            >
              <span class="">{item.jsonData.source.project_title}</span>
              {#if item.jsonData.source.project_id === $selectedProject?.jsonData.source.project_id}
                <icons.Check size={16} class="ml-auto text-primary-500" />
              {/if}
            </button>
          </li>
        {/each}
      </ul>
    </div>
    <div class="flex-grow h-full pl-4">
      <ProjectPanel />
    </div>
  </div>
</div>
