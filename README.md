## embedder\_cpp

Chunking/embedding utility for both code and natural language.  
This is experimental RAG oriented embedder in pure C++.  

### How to build

```
# clone the repository and cd into it. Then:
mkdir build && cd build
cmake ...
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
```./embedder embed --config settings.json```

Check for changes and update only what changed  
```./embedder update```

Continuous monitoring (checks every 60 seconds)  
```./embedder watch 60```

Reclaim space used by deleted index items  
```./embedder compact```

Search nearest neighbours  
```./embedder search "how to optimize C++" --top 10```

Chat with LLM  
```./embedder chat```

Server on custom port with auto-update  
```./embedder serve --port 9000 --watch 60```

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
Edit file `settings.json` to configure settings manually.

Method 2:  
Open `http://localhost:8590/setup` to configure settings interactively.


### REST API endpoints

```
# Health check
curl http://localhost:8590/health

# Search
curl -X POST http://localhost:8590/search \
  -H "Content-Type: application/json" \
  -d '{"query": "optimize performance", "top_k": 5}'

# Embed text (without storing)
curl -X POST http://localhost:8590/embed \
  -H "Content-Type: application/json" \
  -d '{"text": "your text here"}'

# Add document
curl -X POST http://localhost:8590/add \
  -H "Content-Type: application/json" \
  -d '{"content": "document content", "source_id": "doc.txt"}'

# Get stats
curl http://localhost:8590/stats

# Chat completions
curl -N -X POST http://localhost:8590/chat   -H "Content-Type: application/json"   -d '{
    "model": "",
    "messages": [
      {"role": "system", "content": "Keep it short."},
      {"role": "user", "content": "How to get all the chunks from index database?"}
    ],
    "temperature": 0.7
  }'

```

