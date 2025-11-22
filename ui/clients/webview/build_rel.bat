@echo off
echo Building PhenixCode UI...

echo Building SPA client...
cd ..\spa-svelte
rm dist -rf
REM call npm ci
call npm run build

echo Building webview...
cd ..\webview
rm build_rel -rf
mkdir build_rel
cd build_rel
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ..

echo Copying release artifacts to dist folder...
rm dist -rf
mkdir dist
xcopy build_rel\Release\* dist\ /E /Y
rm dist\output.log -f
rm dist\diagnostics.log -f
xcopy appconfig.json dist\

echo Build complete. Package is in dist folder!
