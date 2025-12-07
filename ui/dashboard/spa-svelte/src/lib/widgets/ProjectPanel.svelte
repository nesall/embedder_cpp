<script lang="ts">
  import { Tabs } from "@skeletonlabs/skeleton-svelte";
  import Source from "./project-panels/Source.svelte";
  import Embedding from "./project-panels/Embedding.svelte";
  import Generation from "./project-panels/Generation.svelte";
  import Database from "./project-panels/Database.svelte";
  import Chunking from "./project-panels/Chunking.svelte";
  import Logging from "./project-panels/Logging.svelte";
  import Tokenizer from "./project-panels/Tokenizer.svelte";
  import JSON from "./project-panels/JSON.svelte";
  import { selectedProject } from "../store";
    import Instance from "./project-panels/Instance.svelte";
</script>

{#if $selectedProject}
  <Tabs defaultValue="codebase" class="h-full text-sm">
    <Tabs.List class="">
      <Tabs.Trigger value="codebase" class="text-sm">Sources</Tabs.Trigger>
      <Tabs.Trigger value="generation" class="text-sm">Generation</Tabs.Trigger>
      <Tabs.Trigger value="embedding" class="text-sm">Embedding</Tabs.Trigger>
      <Tabs.Trigger value="chunking" class="text-sm">Chunking</Tabs.Trigger>
      <Tabs.Trigger value="database" class="text-sm">Database</Tabs.Trigger>
      <Tabs.Trigger value="logging" class="text-sm">Logging</Tabs.Trigger>
      <Tabs.Trigger value="tokenizer" class="text-sm">Tokenizer</Tabs.Trigger>
      <Tabs.Trigger value="json" class="text-sm code">JSON</Tabs.Trigger>
      <Tabs.Trigger value="instance" class="text-sm bg-tertiary-100-900">Instance</Tabs.Trigger>
      <Tabs.Indicator />
      <div class="ml-auto flex items-center"></div>
    </Tabs.List>
    <Tabs.Content value="codebase" class="h-0 flex-grow">
      <Source />
    </Tabs.Content>
    <Tabs.Content value="generation" class="h-0 flex-grow">
      <Generation />
    </Tabs.Content>
    <Tabs.Content value="embedding" class="h-0 flex-grow">
      <Embedding />
    </Tabs.Content>
    <Tabs.Content value="chunking" class="h-0 flex-grow">
      <Chunking />
    </Tabs.Content>
    <Tabs.Content value="database" class="h-0 flex-grow">
      <Database />
    </Tabs.Content>
    <Tabs.Content value="logging" class="h-0 flex-grow">
      <Logging />
    </Tabs.Content>
    <Tabs.Content value="tokenizer" class="h-0 flex-grow">
      <Tokenizer />
    </Tabs.Content>
    <Tabs.Content value="json" class="h-0 flex-grow">
      <JSON />
    </Tabs.Content>
    <Tabs.Content value="instance" class="h-0 flex-grow">
      <Instance />
    </Tabs.Content>
  </Tabs>
{:else}
  <div class="h-full flex items-center justify-center">
    <span class="text-surface-400 italic">No project selected</span>
  </div>
{/if}
