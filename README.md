[![License](https://img.shields.io/badge/License-MIT-blue.svg)](./LICENSE)

## embedder\_cpp

embedder_cpp is a local, self-hosted Retrieval-Augmented Generation setup designed for code assistance.
It indexes codebases and documentation to produce context-aware responses to technical queries, drawing on both locally hosted AI models (via HTTP API) and remote cloud models.
The system enables creation of a private, customizable code assistant that can run offline or integrate with cloud LLMs, improving workflow efficiency and long-term knowledge retention.

Configuration is handled through a single JSON file, with no external database requirements.
File-based engines—HNSWLib for vector search and SQLite3 for metadata—manage all storage.

Refer to settings.template.json for an example configuration file and adjust it as needed before launching the embedder.

### How to build

C++ 20 or newer is required.

```bash
# clone the repository and cd into it. Then:
mkdir build && cd build
cmake ..
make
```

### Features overview

Core Functionality:  
* Lightweight tokenization  
* Smart chunking with overlap  
* Local embeddings (llama-server + any choice of embedding model)  
* Both local and remote completion models of your choice
* Fast vector search (Hnswlib with cosine similarity)  
* Metadata storage (SQLite)  
* Incremental updates with file tracking  
* CLI + HTTP API  

API & Server:  
* HTTP API server (httplib)  
* REST endpoints (/api/search, /api/chat, /api/embed)  
* Metrics endpoint (JSON + Prometheus format)  
* Health checks  
* Graceful shutdown  

Security:  
* JWT token authentication  
* Password management (CLI-based)  
* Protected admin endpoints  
* Hashed password storage  

Deployment & Operations:  
* Console setup wizard (interactive)  
* Web setup wizard (password protected)
* Service installation scripts (Windows/Linux/macOS)  
* Structured logging (output.log + diagnostics.log)  
* Auto-start on boot (systemd/NSSM/LaunchAgent)  
* Release packaging (build_rel scripts)  

Configuration:  
* Template-based settings.json  
* Environment variable overrides  
* CLI parameter support  
* Multiple source types (directories, files, URLs)  


### CLI commands

Initial full embed of all sources from settings.json  
```./embedder --config settings.json embed```

Check for changes and update only what changed  
```./embedder update```

Continuous monitoring (checks every 60 seconds)  
```./embedder watch --interval 60```

Reclaim space used by deleted index items  
```./embedder compact```

Search nearest neighbours  
```./embedder search "how to optimize C++" --top 10```

Chat with LLM  
```./embedder chat```

Server on custom port with auto-update  
```./embedder serve --port 9000 --watch --interval 60```

Server without auto-update (manual trigger via /update endpoint)  
```./embedder serve```

Change Password - Method 1: Direct  
```./embeddings_cpp reset-password --pass NewPassword456```

Change Password - Method 2: Interactive (hides password input)  
```./embeddings_cpp reset-password-interactive```

Check password status  
```./embeddings_cpp password-status```


### Editing settings.json

Method 1:  
Edit file `settings.json` to configure settings manually (recommended on first time use).

Method 2:  
Open `http://localhost:8590/setup` to configure settings interactively.


### REST API endpoints

```bash
# Get list of API endpoints
curl -X GET http://localhost:8590/api

# Health check
curl -X GET http://localhost:8590/api/health

# Get document list
curl -X GET http://localhost:8590/api/documents

# Get configuration parameters (full config)
curl -X GET http://localhost:8590/api/setup

# Get available APIs (completion endpoints)
curl -X GET http://localhost:8590/api/settings

# Get running instances (usually one instance per project codebase)
curl -X GET http://localhost:8590/api/instances

# Get database statistics
curl -X GET http://localhost:8590/api/stats

# Get metrics (JSON)
curl -X GET http://localhost:8590/api/metrics

# Get Prometheus metrics
curl -X GET http://localhost:8590/metrics

# Setup configuration (POST)
curl -X POST http://localhost:8080/api/setup \
  -H "Authorization: Basic $(echo -n "username:password" | base64)" \
  -H "Content-Type: application/json" \
  -d '{
    "embedding": {...},
    "generation": {...},
    "database": {...},
    "chunking": {...}
  }'

# Search
curl -X POST http://localhost:8590/api/search \
  -H "Content-Type: application/json" \
  -d '{"query": "optimize performance", "top_k": 5}'

# Generate embeddings (without storing)
curl -X POST http://localhost:8590/api/embed \
  -H "Content-Type: application/json" \
  -d '{"text": "your text here"}'

# Add document
curl -X POST http://localhost:8080/api/documents \
  -H "Content-Type: application/json" \
  -d '{
    "content": "your document content",
    "source_id": "document_source_id"
  }'

# Trigger a manual update
curl -X POST http://localhost:8080/api/update

# Chat with optional context (streaming)
curl -X POST http://localhost:8080/api/chat \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [
      {"role": "system", "content": "Keep it short."},
      {"role": "user", "content": "What is the capital of France?"}
    ],
    "sourceids": [
      "../embedder_cpp/src/main.cpp",
      "../embedder_cpp/include/settings.h"
    ],
    "attachments": [
      { "filename": "filename1.cpp", "content": "..text file content 1.."},
      { "filename": "filename2.cpp", "content": "..text file content 2.."}
    ],
    "temperature": 0.2,
    "max_tokens": 800,
    "targetapi": "xai",
    "ctxratio": 0.5,
    "attachedonly": false
  }'

# Initiate server shutdown that was started with an app key e.g. ./embeddings_cpp serve --appkey abc123
curl -X POST http://localhost:8590/api/shutdown \
  -H "X-App-Key: abc123" \
  -d '{}'  

# Authenticate  
curl -X POST http://localhost:8080/api/authenticate \
  -H "Authorization: Basic $(echo -n "username:password" | base64)"

  
```

