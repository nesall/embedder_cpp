@echo off
SETLOCAL ENABLEDELAYEDEXPANSION

SET PID_FILE=process.pid
SET BINARY=embeddings_cpp.exe
SET ARGS=serve --port 8590 --watch --interval 60

REM Check if PID file exists and process is running
IF EXIST %PID_FILE% (
    FOR /F "tokens=*" %%p IN (%PID_FILE%) DO (
        tasklist /FI "PID eq %%p" | find "%%p" >nul
        IF !ERRORLEVEL! EQU 0 (
            echo Service already running with PID %%p
            exit /b 1
        )
    )
)

REM Start the process in background
echo Starting PhenixCode RAG manually...
start "" /B %BINARY% %ARGS%
REM Give it a moment to start and get PID
timeout /t 1 >nul

REM Get the PID of the newly started process
for /f "tokens=2" %%a in ('tasklist /FI "IMAGENAME eq %BINARY%" /FO LIST ^| find "PID:"') do (
    echo %%a > %PID_FILE%
    echo Started with PID %%a
    goto :done
)

:done
ENDLOCAL
