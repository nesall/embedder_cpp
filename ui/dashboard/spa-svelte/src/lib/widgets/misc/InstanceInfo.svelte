<script lang="ts">
  import { onMount } from "svelte";
  import * as icons from "@lucide/svelte";
  import type { InstanceItem } from "../../../app";
  import StatCard from "./StatCard.svelte";
  import DetailRow from "./DetailRow.svelte";

  interface Props {
    instance?: InstanceItem;
  }

  let { instance }: Props = $props();

  onMount(() => {});

  function calculateUptime(startTime: number): string {
    const now = Math.floor(Date.now() / 1000);
    const durationSeconds = now - startTime;
    const hours = Math.floor(durationSeconds / 3600);
    const minutes = Math.floor((durationSeconds % 3600) / 60);
    const seconds = durationSeconds % 60;

    if (hours > 0) return `${hours}h ${minutes}m`;
    if (minutes > 0) return `${minutes}m ${seconds}s`;
    return `${seconds}s`;
  }

  function getStatusStyle(status: InstanceItem["status"]) {
    switch (status) {
      case "healthy":
        return { color: "bg-green-500", icon: icons.CircleCheck };
      case "unhealthy":
        return { color: "bg-red-500", icon: icons.CircleX };
      case "starting":
        return { color: "bg-yellow-500", icon: icons.Hourglass };
      default:
        return { color: "bg-gray-500", icon: icons.CircleQuestionMark };
    }
  }

  const statusStyle = $derived(getStatusStyle(instance ? instance.status : "unknown"));
</script>

{#if instance}
  <div class="h-full p-4 overflow-auto">
    <div class="rounded-xl shadow-lg bg-white dark:bg-gray-800 p-6 space-y-6 max-w-4xl mx-auto">
      <!-- Header and Status -->
      <div class="flex justify-between items-center border-b pb-4">
        <h1 class="text-2xl font-extrabold text-gray-900 dark:text-white flex items-center gap-3">
          <icons.Activity size={28} class="text-blue-500" />
          Service Status: {instance.name}
        </h1>

        <div class="flex items-center space-x-2">
          <span class="text-sm font-semibold uppercase text-gray-500 dark:text-gray-400">Status</span>
          <div
            class="px-3 py-1 text-sm font-bold text-white rounded-full flex items-center gap-1"
            class:bg-green-500={instance.status === "healthy"}
            class:bg-red-500={instance.status === "unhealthy"}
            class:bg-yellow-500={instance.status === "starting"}
          >
            <statusStyle.icon size={14} />
            <span>{instance.status.toUpperCase()}</span>
          </div>
        </div>
      </div>

      <!-- Key Metrics Grid -->
      <div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        <StatCard label="Project ID" value={instance.project_id} />
        <StatCard label="Host:Port" value={`${instance.host}:${instance.port}`} />
        <StatCard label="Process ID" value={String(instance.pid)} />
        <StatCard label="Last Heartbeat" value={instance.last_heartbeat_str} />
        <StatCard label="Service Uptime" value={calculateUptime(instance.started_at)} />
        <StatCard label="Started At" value={instance.started_at_str} />
      </div>

      <!-- Detail Section: Paths and Config -->
      <div class="pt-4 border-t">
        <h3 class="text-xl font-semibold mb-3 text-gray-900 dark:text-white">Configuration Details</h3>
        <div class="space-y-3">
          <DetailRow icon={icons.Settings} label="Config File:" value={instance.config} />
          <DetailRow icon={icons.Folder} label="Working Directory:" value={instance.cwd} />
          <DetailRow icon={icons.FingerprintPattern} label="Instance ID:" value={instance.id} />
        </div>
      </div>
    </div>
  </div>
{/if}
