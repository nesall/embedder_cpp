<script lang="ts">
  import { tick } from "svelte";

  interface Props {
    name: string;
    checked: boolean;
    loading?: boolean;
    id?: string;
    title?: string;
    onClose?: () => void;
    onToggle?: (b: boolean) => void;
  }
  let {
    name = "(placeholder)",
    checked = $bindable(false),
    loading = false,
    title,
    id = Math.random().toString(36).substring(2, 15),
    onClose,
    onToggle = (b: boolean) => {},
  }: Props = $props();

  $effect(() => {
    // clog("Checkbox:", { name, checked, loading });
  });
</script>

<div
  class="flex items-center space-x-1 whitespace-nowrap border
        {checked ? 'border-primary-100-900' : 'border-surface-100-900'}
        {checked ? 'bg-primary-50-950' : 'bg-surface-50-950'}
        rounded p-0 pr-1 2h-5 w-fit"
  title={title || name}
>
  <input
    type="checkbox"
    id={`context-file-${id}`}
    class="checkbox w-4 h-4 p-0 m-0 accent-primary-500"
    bind:checked
    disabled={loading}
    onchange={() => tick().then(() => onToggle(checked))}
  />
  <label
    for={`context-file-${id}`}
    class="text-xs select-none pl-1 cursor-pointer {!checked ? 'text-surface-500 italic' : ''}"
  >
    {name}
  </label>
  {#if onClose}
    <button
      type="button"
      class="btn btn-sm w-4 h-4 px-0 hover:text-primary-500 rounded-full"
      title={`Remove ${name}`}
      aria-label={`Remove ${name}`}
      onclick={onClose}
    >
      ✕
    </button>
  {/if}
</div>
