@echo off
setlocal

set "NAME=phenixcode-v1.0.0-win64"
set "EMBEDDER=."
set "WEBVIEW=ui\clients\webview"

rem ---------- build embedder ----------
pushd "%EMBEDDER%"
call build_rel.bat
popd
echo FINISHED %EMBEDDER%

rem ---------- build webview ----------
pushd "%WEBVIEW%"
call build_rel.bat
popd

rem ---------- package ----------
rmdir /s /q "%NAME%" 2>nul
mkdir "%NAME%"

xcopy "%EMBEDDER%\dist\*"   "%NAME%\" /E /Y
xcopy "%WEBVIEW%\dist\*"    "%NAME%\" /E /Y

del /f /q "%NAME%.zip" 2>nul
echo %NAME%.zip...
powershell -NoProfile -Command "Compress-Archive -Path '%NAME%' -DestinationPath '%NAME%.zip' -Force"

rmdir /s /q "%NAME%"
echo Package '%NAME%.zip' ready.

endlocal