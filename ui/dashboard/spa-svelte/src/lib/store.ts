import { writable } from 'svelte/store';
import type { ProjectItem, SettingsJsonType } from '../app';

// export const selectedJsonSettings = writable<SettingsJsonType | null>();
export const selectedProject = writable<ProjectItem>();
