declare interface Window {
  apiServerUrl: string | undefined;
  cppApi: {
    setServerUrl: (url: string | undefined) => any;
    getServerUrl: () => Promise<string | undefined>;
    setPersistentKey: (key: string, value: string) => Promise<void>;
    getPersistentKey: (key: string) => Promise<string | null>;
    getSettingsFileProjectId: (path: string) => Promise<string | null>;
    startEmbedder: (executablePath: string, settingsFilePath: string) => Promise<{ status: string; message: string, appKey: string, projectId: string }>;
    stopEmbedder: (appKey: string, host: string, port: number) => Promise<{ status: string; message: string }>;
  };
  // hljs: {
  //   highlightAll: () => any;
  // }
  HLJS_CUSTOM: {
    initHljs: () => any;
    hlAuto: (s: string, lang?: string) => string;
  }
}