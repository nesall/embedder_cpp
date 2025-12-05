// import type { ProjectItem } from "./app";

declare interface Window {
  cppApi: {
    setPersistentKey: (key: string, value: string) => Promise<void>;
    getPersistentKey: (key: string) => Promise<string | null>;
    getSettingsFileProjectId: (path: string) => Promise<string | null>;
    startEmbedder: (executablePath: string, settingsFilePath: string) => Promise<{ status: string; message: string, appKey: string, projectId: string }>;
    stopEmbedder: (appKey: string, host: string, port: number) => Promise<{ status: string; message: string }>;

    getPojectList: () => Promise<ProjectItem[]>;
    savePojectSettings: (project: ProjectItem) => Promise<{ status: string; message: string }>;
    // getPojectSettings: (projectId: string) => Promise<SettingsJsonType | null>;
  };
}