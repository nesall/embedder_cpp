@echo off
setlocal

if "%~1"=="" (
    echo Usage: package.bat version
    echo Example: package.bat 1.0.1
    exit /b 1
)

set "VER=%~1"
set "NAME=phenixcode-v%VER%-win64"

echo VER=%VER%
echo NAME=%NAME%

set "EMBEDDER=."
set "CLIENT=ui\clients\webview"
set "DASHBOARD=ui\dashboard\webview"

rem ---------- build embedder ----------
pushd "%EMBEDDER%"
call build_rel.bat
popd
echo FINISHED %EMBEDDER%

rem ---------- build client ----------
pushd "%CLIENT%"
call build_rel.bat
popd

rem ---------- build dashboard ----------
pushd "%DASHBOARD%"
call build_rel.bat
popd

rem ---------- package ----------
rmdir /s /q "%NAME%" 2>nul
mkdir "%NAME%"

xcopy "%EMBEDDER%\dist\*"   "%NAME%\" /E /Y
xcopy "%CLIENT%\dist\*"    "%NAME%\" /E /Y
xcopy "%DASHBOARD%\dist\*"    "%NAME%\" /E /Y

del /f /q "%NAME%.zip" 2>nul
echo %NAME%.zip...
powershell -NoProfile -Command "Compress-Archive -Path '%NAME%' -DestinationPath '%NAME%.zip' -Force"

rmdir /s /q "%NAME%"
echo Package '%NAME%.zip' ready.

endlocal