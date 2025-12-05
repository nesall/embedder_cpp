<script lang="ts">
    import CentralWidget from "./lib/widgets/CentralWidget.svelte";
  import Statusbar from "./lib/widgets/Statusbar.svelte";
    import Toolbar from "./lib/widgets/Toolbar.svelte";
</script>

<main
  class="mx-auto flex flex-col space-y-2 items-center justify-center
    w-[100vw] h-[100vh] min-w-sx min-h-sx max-h-[100vh]"
>
  <div class="p-4 m-0 flex flex-col w-full h-full gap-2">
    <!-- <Toolbar /> -->
    <div class="flex-grow w-full h-0 mb-[-0.5rem]">
      <!-- Main content goes here -->
       <CentralWidget />
    </div>
  </div>
  <div class="w-full">
    <Statusbar />
  </div>
</main>
