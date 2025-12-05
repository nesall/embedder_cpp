import type { ProjectItem, SettingsJsonType } from "../app";
import { createToaster } from '@skeletonlabs/skeleton-svelte';

export const toaster = createToaster();

export function jsonDeepCopy<T>(obj: T): T {
  return JSON.parse(JSON.stringify(obj));
}

export const defaultJsonSettings: SettingsJsonType =
{
  "tokenizer": {
    "config_path": "./bge_tokenizer.json"
  },
  "embedding": {
    "apis": [
      {
        "api_key": "",
        "api_url": "http://127.0.0.1:8583/embedding",
        "id": "local",
        "model": "bge-base-v1.5",
        "name": "llamacpp-server",
        "document_format": "{}",
        "query_format": "Represent this sentence for searching relevant passages: {}"
      }
    ],
    "current_api": "local",
    "batch_size": 4,
    "timeout_ms": 30000,
    "retry_attempts": 3,
    "top_k": 5,
    "prepend_label_format": "[Source: {}]\n"
  },
  "generation": {
    "apis": [
      {
        "api_key": "${MISTRAL_API_KEY}",
        "api_url": "https://api.mistral.ai/v1/chat/completions",
        "id": "mistral-devstral",
        "model": "devstral-small-latest",
        "name": "Mistral",
        "pricing_tpm": {
          "input": 0.1,
          "output": 0.3
        },
        "context_length": 128000
      },
      {
        "api_key": "${GEMINI_API_KEY}",
        "api_url": "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions",
        "id": "gemini-2.0-flash",
        "model": "gemini-2.0-flash",
        "name": "Gemini",
        "pricing_tpm": {
          "input": 0.1,
          "output": 0.4
        },
        "context_length": 1000000
      },
      {
        "api_key": "${DEEPSEEK_API_KEY}",
        "api_url": "https://api.deepseek.com/chat/completions",
        "id": "deepseek",
        "model": "deepseek-chat",
        "name": "DeepSeek",
        "pricing_tpm": {
          "cached_input": 0.028,
          "input": 0.28,
          "output": 0.42
        },
        "context_length": 128000
      },
      {
        "api_key": "${XAI_API_KEY}",
        "api_url": "https://api.x.ai/v1/chat/completions",
        "id": "xai",
        "model": "grok-4-fast-non-reasoning",
        "name": "X AI",
        "pricing_tpm": {
          "cached_input": 0.05,
          "input": 0.2,
          "output": 0.5
        },
        "context_length": 2000000
      },
      {
        "api_key": "${OPENAI_API_KEY}",
        "api_url": "https://api.openai.com/v1/chat/completions",
        "id": "openai-4o-mini",
        "model": "gpt-4o-mini",
        "name": "OpenAI",
        "max_tokens_name": "max_completion_tokens",
        "pricing_tpm": {
          "cached_input": 0.075,
          "input": 0.15,
          "output": 0.6
        },
        "context_length": 128000
      }
    ],
    "current_api": "mistral-devstral",
    "timeout_ms": 120000,
    "max_chunks": 7,
    "max_full_sources": 2,
    "max_related_per_source": 3,
    "max_context_tokens": 64000,
    "default_temperature": 0.1,
    "default_max_tokens": 2048,
    "default_max_tokens_name": "max_tokens",
    "prepend_label_format": "[Source: {}]\n",
    "excerpt": {
      "enabled": true,
      "min_chunks": 3,
      "max_chunks": 15,
      "threshold_ratio": 0.75
    }
  },
  "database": {
    "sqlite_path": "./db_metadata.db",
    "index_path": "./db_embeddings.index",
    "vector_dim": 768,
    "max_elements": 100000,
    "distance_metric": "cosine",
    "_comment": "For distance_metric use either cosine (default) or l2"
  },
  "chunking": {
    "semantic": true,
    "nof_min_tokens": 50,
    "nof_max_tokens": 450,
    "overlap_percentage": 0.2
  },
  "source": {
    "_comment_project_id": "Leave empty to auto-generate from config path, or set a custom stable project_id",
    "project_id": "",
    "project_title": "Default Project Title",
    "project_description": "",
    "paths": [
      {
        "exclude": [
          "*/dist/*",
          "*/test/*",
          "*/3rdparty/*"
        ],
        "extensions": [],
        "path": "./",
        "recursive": true,
        "type": "directory"
      }
    ],
    "default_extensions": [".c", ".cpp", ".h", ".hpp", ".py", ".js", ".ts", ".java", ".rs", ".cs", ".xaml", ".php", ".jsp", ".html", ".css", ".md"],
    "global_exclude": [
      "*/node_modules/*", "*.min.js", "*.log", "*/build/*",
      "*/.git/*", ".gitignore", "*/CVS/*",
      "*/debug/*", "*/release/*", "*/lib/*", "*/docker/*",
      "*/fonts/*", "*/images/*", "*/test/*", "*/tests/*",
      "*/example/*", "*/examples/*", "*/obj/*", "*/build_dbg/*", "*/build_rel/*"
    ],
    "encoding": "utf-8",
    "max_file_size_mb": 10
  },
  "logging": {
    "log_to_console": true,
    "log_to_file": true,
    "logging_file": "embedder.log",
    "diagnostics_file": "embedder_d.log"
  }
};



export const Consts = {
  DarkOrLightKey: "darkOrLight",
  EmbedderExecutablePath: "EmbedderExecutablePath",
  EmbedderSettingsFilePaths: "EmbedderSettingsFilePaths"
};

export async function setPersistentKey(key: string, value: string, sendToCpp = true) {
  try {
    console.log(`setPersistentKey ${key}`, value, sendToCpp, window.cppApi);
    localStorage.setItem(key, value);
    if (sendToCpp && window.cppApi) {
      await window.cppApi.setPersistentKey(key, value);
      console.log(`Saved persistent key ${key} to C++`, value);
    }
  } catch (error) {
    console.log(`Unable to set persistent key ${key}`, error)
  }
}

export async function getPersistentKey(key: string, readFromCpp = true): Promise<string | null> {
  try {
    console.log(`getPersistentKey ${key}`, readFromCpp, window.cppApi);
    let val = localStorage.getItem(key);
    if (readFromCpp && window.cppApi) {
      val = await window.cppApi.getPersistentKey(key);
      console.log(`Loaded persistent key ${key} from C++`, val);
      if (val != null) {
        localStorage.setItem(key, val);
      }
    }
    return val;
  } catch (error) {
    console.log(`Unable to get persistent key ${key}`, error)
  }
  return null;
}
// Mock data for testing without C++ backend
let mockProjects: ProjectItem[] = [
  {
    settingsFilePath: "C:/Users/Arman/workspace/projects/alpha/settings-embcpp.json",
    jsonData: {
      ...defaultJsonSettings,
      source: {
        ...defaultJsonSettings.source,
        project_id: "project1",
        project_title: "phenixcode",
      }
    }
  },
  {
    settingsFilePath: "C:/Users/Arman/workspace/projects/alpha/settings-vsix.json",
    jsonData: {
      ...defaultJsonSettings,
      source: {
        ...defaultJsonSettings.source,
        project_id: "project2",
        project_title: "ChatAssistantVSIX",
      }
    }
  }
];

// Return mock data for testing
export async function test_getProjectList() {
  console.log("[mock] fetching projects", mockProjects);
  return mockProjects;
}

// export async function test_getPojectSettings(projectId: string): Promise<SettingsJsonType | null> {
//   if (projectId === "project1") {
//     return mockProjects[0].jsonData;
//   } else if (projectId === "project2") {
//     return mockProjects[1].jsonData;
//   }
//   return null;
// }

export async function test_savePojectSettings(project: ProjectItem): Promise<{ status: string; message: string }> {
  // if (projectId === "project1") {
  //   mockProjects[0].jsonData = settings;
  //   return { status: "success", message: "Project 1 settings updated." };
  // } else if (projectId === "project2") {
  //   mockProjects[0].jsonData = settings;
  // }
  console.log("[mock] saving project settings", project);
  return { status: "success", message: "Project 2 settings updated." };
  // return { status: "error", message: "Project not found." };
}