<script lang="ts">
  import * as icons from "@lucide/svelte";

  interface Props {
    hidden?: boolean;
    text: string;
    onChange?: (hidden: boolean) => void;
  }
  let { hidden = $bindable(false), text = "", onChange = () => {} }: Props = $props();
  function onChangeInternal() {
    hidden = !hidden;
    if (onChange) onChange(hidden);
  }
</script>

<button
  type="button"
  class="btn btn-sm flex items-center justify-start preset-tonal-secondary {hidden ? '' : 'rounded-b-none'}"
  onclick={onChangeInternal}
>
  {#if hidden}
    <icons.ChevronDown />
    <span class="ml-2 italic">{text}</span>
  {:else}
    <icons.ChevronUp />
    <span class="ml-2">{text}</span>
  {/if}
</button>
