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

rm -rf %NAME%
mkdir %NAME%

cp %EMBEDDER%/dist/* %NAME% -rf
cp %WEBVIEW%/dist/* %NAME% -rf

rm -f %NAME%.zip
echo %NAME%.zip...
powershell -NoProfile -Command "Compress-Archive -Path '%NAME%' -DestinationPath '%NAME%.zip' -Force"

rm -rf %NAME%/
echo Package '%NAME%.zip' ready.

endlocal
