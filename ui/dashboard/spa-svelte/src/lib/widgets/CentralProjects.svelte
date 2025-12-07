<script lang="ts">
  import { onMount } from "svelte";
  import * as icons from "@lucide/svelte";
  import ProjectPanel from "./ProjectPanel.svelte";
  import {
    hasRunningInstance,
    helper_createProject,
    helper_deleteProject,
    helper_getProjectList,
    helper_importProject,
    toaster,
  } from "../utils";
  import type { ProjectItem } from "../../app";
  import { instances, selectedProject, projectList } from "../store";
  import { slide } from "svelte/transition";
  // import { Dialog, Portal } from "@skeletonlabs/skeleton-svelte";

  // let openImportState = $state(false);

  onMount(() => {
    helper_getProjectList().then((v) => projectList.set(v));
  });

  async function onItemClick(item: ProjectItem) {
    // const settings = await test_getPojectSettings(item.jsonData.source.project_id);
    // if ($selectedProject) {
    //   console.log("Saving selectedJsonSettings");
    //   test_setPojectSettings($selectedProject.jsonData.source.project_id, $selectedJsonSettings);
    // }
    console.log("Switching to project", $state.snapshot(item.jsonData));
    // selectedJsonSettings.set(settings);
    selectedProject.set(item);
  }

  async function onAddProject() {
    try {
      selectedProject.set(await helper_createProject());
      projectList.set(await helper_getProjectList());
      toaster.success({ title: "Project created successfully." });
    } catch (error) {
      toaster.error({ title: "Failed to create project." });
    }
  }

  async function onDeleteProject() {
    try {
      if (!$selectedProject) return;
      await helper_deleteProject($selectedProject);
      projectList.set(await helper_getProjectList());
      selectedProject.set(null);
      toaster.success({ title: "Project deleted successfully." });
    } catch (error) {
      toaster.error({ title: "Failed to delete project." });
    }
  }

  async function onImportProject() {
    if (window.cppApi) {
      window.cppApi
        .pickSettingsJsonFile()
        .then((config: { project_id: string; path: string } | null) => {
          if (config) {
            helper_importProject(config.project_id, config.path).then((res) => {
              helper_getProjectList().then((v) => projectList.set(v));
              if (res.status === "success") {
                toaster.success({
                  title: "Import Successful",
                  description: `Project ${config.project_id} imported into Projects.`,
                });
              } else {
                console.log("Failed to import project", config.project_id, res.message);
                toaster.error({
                  title: "Failed to import project.",
                  description: `Project ${config.project_id} failed with error ${res.message}`,
                });
              }
            });
          }
        })
        .catch((err) => {
          toaster.error({ title: "Unable to pick a settings file.", description: err.message || err });
        });
    } else {
      toaster.info({ title: "Import not available in web mode." });
      // openImportState = true;
    }
  }
</script>

<div class="h-full">
  <div class="h-full flex">
    <div class="h-full max-w-xs flex flex-col space-y-1">
      <div class="flex items-center gap-2">
        <button
          type="button"
          class="btn-icon btn-sm preset-filled-primary-500"
          title="Add new project"
          onclick={onAddProject}
        >
          <icons.Plus />
        </button>
        <button
          type="button"
          class="btn-icon btn-sm preset-filled-primary-500"
          title="Import a project"
          onclick={onImportProject}
        >
          <icons.Import />
        </button>
        <button
          type="button"
          class="btn-icon btn-sm preset-filled-error-500 ml-auto"
          title="Delete selected project"
          disabled={!$selectedProject}
          onclick={onDeleteProject}
        >
          <icons.Trash2 />
        </button>
      </div>
      <ul class="w-full h-full p-1 shadow overflow-y-auto border border-surface-200-800 rounded min-w-64">
        <div class="bg-surface-100-900 p-2 mb-2">View/Edit Projects</div>
        {#each $projectList as item (item.jsonData.source.project_id)}
          <li
            class="hover:bg-surface-200-800 px-2 flex items-center space-x-2 border-b border-surface-200-800"
            transition:slide
          >
            <button
              type="button"
              class="btn btn p-1 w-full flex items-center space-x-2 justify-start text-sm
              {item.jsonData.source.project_id === $selectedProject?.jsonData.source.project_id ? 'font-bold' : ''}
              "
              onclick={() => onItemClick(item)}
            >
              {#if hasRunningInstance(item.jsonData.source.project_id, $instances)}
                <span class="font-monospace text-xs bg-success-300-700 rounded px-2 w-8 font-bold">on</span>
              {:else}
                <span class="font-monospace text-xs bg-surface-100-900 rounded text-surface-800-200 px-2 w-8">off</span>
              {/if}
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
<!-- 
<Dialog open={openImportState} onOpenChange={(e) => (openImportState = e.open)}>
  <Portal>
    <Dialog.Backdrop class="" />
    <Dialog.Positioner class="fixed inset-0 z-50 flex justify-center items-center">
      <Dialog.Content class="card bg-surface-100-900 w-md p-4 space-y-2 shadow-xl">
        <Dialog.Title class="text-lg font-bold">Project Import</Dialog.Title>
        <hr class="hr" />
        <Dialog.Description>
          <div class="flex flex-col space-y-4">
            Enter absulote path of a settings JSON file
            <input type="text" class="input w-full" placeholder="Path to project config JSON file" />
          </div>
        </Dialog.Description>
        <Dialog.CloseTrigger class="btn preset-filled w-full">Close</Dialog.CloseTrigger>
      </Dialog.Content>
    </Dialog.Positioner>
  </Portal>
</Dialog> -->
