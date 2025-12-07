<script lang="ts">
  interface Props {
    icon: any; // Svelte component
    label: string;
    value: string;
  }

  let { icon: Icon, label = "", value = "" }: Props = $props();
</script>

<div class="flex items-start items-center gap-3 p-2 bg-gray-50 dark:bg-gray-700 rounded">
  <Icon class="text-gray-500" />
  <span class="font-semibold text-gray-700 dark:text-gray-300">{label}</span>
  <code class="text-sm text-gray-800 dark:text-gray-200 break-all bg-gray-100 dark:bg-gray-900 px-2 py-0.5 rounded-sm"
    >{value}</code
  >
</div>
