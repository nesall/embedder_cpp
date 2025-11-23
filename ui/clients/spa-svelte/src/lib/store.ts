import { writable } from 'svelte/store';

export const temperature = writable<number>(0.1);
export const settings = writable<SettingsType>({ completionApis: [], currentApi: "" });
export const messages = writable<ChatMessage[]>([]);
export const instances = writable<AppInstance[]>([]);
export const curInstance = writable<string>("");
export const contextSizeRatio = writable<number>(0.7);
export const bApisGroupedByLabel = writable<boolean>(false);
export const bApisSortedByPrice = writable<boolean>(true);
// export const embedderExecutablePath = writable<string>("");
// export const embedderSettingsFilePaths = writable<string[]>([]);
