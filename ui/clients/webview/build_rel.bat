@echo off
echo Building PhenixCode UI...

echo Building SPA client...
cd ..\spa-svelte
rmdir /s /q dist
rmdir /s /q node_modules
call npm install
call npm run build

echo Building webview...
cd ..\webview
rmdir /s /q build_rel
mkdir build_rel
cd build_rel
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ..

echo Copying release artifacts to dist folder...
rmdir /s /q dist
mkdir dist
xcopy build_rel\Release\* dist\ /E /Y
del /f /q dist\output.log
del /f /q dist\diagnostics.log
xcopy assets\appconfig.json dist\

echo Build complete. Package is in dist folder!
