@echo off
setlocal

set "PID_FILE=process.pid"

if not exist "%PID_FILE%" (
    echo PID file not found. Is the service running?
    exit /b 1
)

:: Read PID (expecting a single line in the file)
set /p PID=<"%PID_FILE%"

:: Sanity check
if "%PID%"=="" (
    echo PID file is empty or invalid.
    exit /b 1
)

tasklist /FI "PID eq %PID%" | find "%PID%" >nul
if errorlevel 1 (
    echo Process %PID% not running.
    del "%PID_FILE%"
    exit /b 0
)

echo Stopping PhenixCode RAG (PID %PID%)...
taskkill /PID %PID% >nul 2>&1
timeout /t 2 >nul

tasklist /FI "PID eq %PID%" | find "%PID%" >nul
if not errorlevel 1 (
    echo Process did not stop, forcing termination...
    taskkill /PID %PID% /F >nul 2>&1
)

del "%PID_FILE%"
echo Stopped.
endlocal
