@echo off
REM uninstall-service.bat

echo Uninstalling Embedder RAG Service...

REM Check if NSSM is present
if not exist "nssm.exe" (
    echo nssm.exe not found in current directory.
    echo Please place uninstall-service.bat in the same folder as nssm.exe.
    pause
    exit /b 1
)

REM Stop and remove the service
nssm stop PhenixCodeRAG
nssm remove PhenixCodeRAG confirm

echo Service uninstalled successfully.
pause
