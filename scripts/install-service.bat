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
nssm install PhenixCodeRAG "%CD%\embeddings_cpp.exe" serve --port 8081 --watch 60
nssm set PhenixCodeRAG AppDirectory "%CD%"
nssm set PhenixCodeRAG DisplayName "PhenixCode RAG Service"
nssm set PhenixCodeRAG Description "Local RAG embeddings and search service"
nssm set PhenixCodeRAG Start SERVICE_AUTO_START

REM Start service
nssm start PhenixCodeRAG

echo Service installed and started!
echo View status: nssm status PhenixCodeRAG
echo Stop service: nssm stop PhenixCodeRAG
echo Uninstall: nssm remove PhenixCodeRAG confirm
pause