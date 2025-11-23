@echo off
setlocal

set NAME=phenixcode-v1.0.0-win64
set EMBEDDER=./
set WEBVIEW=ui/clients/webview

cd %EMBEDDER%
call build_rel.bat
echo FINISHED %EMBEDDER%

cd %WEBVIEW%
call build_rel.bat
cd ../../..

rmdir /s /q %NAME%
mkdir %NAME%

xcopy %EMBEDDER%/dist/* %NAME% /E /Y
xcopy %WEBVIEW%/dist/* %NAME% /E /Y

del /f /q %NAME%.zip 2>nul
echo %NAME%.zip...
powershell -NoProfile -Command "Compress-Archive -Path '%NAME%' -DestinationPath '%NAME%.zip' -Force"

rmdir /s /q %NAME%/
echo Package '%NAME%.zip' ready.

endlocal
