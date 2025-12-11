<script lang="ts">
  import { onMount } from "svelte";
  import * as icons from "@lucide/svelte";
  import { Consts, getPersistentKey, setPersistentKey } from "../utils";

  let embedderExecutablePath = $state("");

  onMount(() => {
    getPersistentKey(Consts.EmbedderExecutablePath).then((path) => {
      embedderExecutablePath = path || "./phenixcode-core";
      if (!path) {
        setPersistentKey(Consts.EmbedderExecutablePath, embedderExecutablePath);
      }
      console.log("embedderExecutablePath", $state.snapshot(embedderExecutablePath));
    });
  });

  function onCorePathChange(e: Event) {
    const ev = e as InputEvent;
    if (ev && ev.target) {
      setPersistentKey(Consts.EmbedderExecutablePath, (ev.target as HTMLInputElement).value);
      embedderExecutablePath = (ev.target as HTMLInputElement).value;
    }
  }
</script>

<div>
  <label class="label">
    <span class="label-text">PhenixCode Executable path</span>
    <div class="flex items-center space-x-1">
      <input
        type="text"
        class="input max-w-xl {!embedderExecutablePath ? 'outline-2 outline-red-500' : ''}"
        oninput={onCorePathChange}
        value={embedderExecutablePath}
      />
      <!-- <button type="button" class="btn preset-tonal" title="Browse for the core executable" onclick={onBrowse}>
        ...
      </button> -->
    </div>
    {#if !embedderExecutablePath}
      <div class="text-xs italic">(Invalid state - cannot be empty)</div>
    {/if}
  </label>
</div>

<style>
  .label {
    text-align: left;
  }
</style>
