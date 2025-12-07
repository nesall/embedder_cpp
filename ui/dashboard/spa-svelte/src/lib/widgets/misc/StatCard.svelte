<script lang="ts">
  interface Props {
    label: string;
    value: string;
  }

  let { label = "", value = "" }: Props = $props();
</script>

<div class="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg shadow-inner">
  <span class="text-sm font-medium text-gray-500 dark:text-gray-400 block">{label}</span>
  <span class="text-xl font-extrabold text-gray-900 dark:text-white">{value}</span>
</div>
