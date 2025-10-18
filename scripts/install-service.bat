@echo off
REM install-service.bat

echo Installing Embedder RAG Service...

REM Download NSSM if not present
if not exist "nssm.exe" (
    echo Downloading NSSM...
    powershell -Command "Invoke-WebRequest -Uri https://nssm.cc/release/nssm-2.24.zip -OutFile nssm.zip"
    powershell -Command "Expand-Archive -Path nssm.zip -DestinationPath ."
    copy nssm-2.24\win64\nssm.exe .
)

REM Install service
nssm install EmbedderCoreRAG "%CD%\rag_embedder_svc.exe" serve --port 8081 --watch 60
nssm set EmbedderCoreRAG AppDirectory "%CD%"
nssm set EmbedderCoreRAG DisplayName "Embedder RAG Service"
nssm set EmbedderCoreRAG Description "Local RAG embeddings and search service"
nssm set EmbedderCoreRAG Start SERVICE_AUTO_START

REM Start service
nssm start EmbedderCoreRAG

echo Service installed and started!
echo View status: nssm status EmbedderCoreRAG
echo Stop service: nssm stop EmbedderCoreRAG
echo Uninstall: nssm remove EmbedderCoreRAG confirm
pause