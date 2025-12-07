
declare interface Window {
  cppApi: {
    setPersistentKey: (key: string, value: string) => Promise<void>;
    getPersistentKey: (key: string) => Promise<string | null>;
    getSettingsFileProjectId: (path: string) => Promise<string | null>;
    startEmbedder: (executablePath: string, settingsFilePath: string) => Promise<{ status: string; message: string, appKey: string, projectId: string }>;
    stopEmbedder: (appKey: string, host: string, port: number) => Promise<{ status: string; message: string }>;

    createProject: () => Promise<ProjectItem>;
    deleteProject: (project: ProjectItem) => Promise<{ status: string; message: string }>;
    importProject: (projectId: string, configPath: string) => Promise<{ status: string; message: string }>;
    getProjectList: () => Promise<ProjectItem[]>;
    saveProject: (project: any) => Promise<{ status: string; message: string }>;
    getInstances: () => Promise<any[]>;
    pickSettingsJsonFile: () => Promise<{ project_id: string, path: string } | null>;
  };
}