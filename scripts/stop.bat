@echo off
SETLOCAL ENABLEDELAYEDEXPANSION

SET PID_FILE=process.pid

IF NOT EXIST %PID_FILE% (
    echo PID file not found. Is the service running?
    exit /b 1
)

FOR /F "tokens=*" %%p IN (%PID_FILE%) DO (
    tasklist /FI "PID eq %%p" | find "%%p" >nul
    IF !ERRORLEVEL! NEQ 0 (
        echo Process %%p not running.
        del %PID_FILE%
        exit /b 0
    )

    echo Stopping Embedder RAG (PID %%p)...
    taskkill /PID %%p
    timeout /t 2 >nul
    tasklist /FI "PID eq %%p" | find "%%p" >nul
    IF !ERRORLEVEL! EQU 0 (
        echo Process did not stop, forcing termination...
        taskkill /PID %%p /F
    )
)

del %PID_FILE%
echo Stopped.
ENDLOCAL
