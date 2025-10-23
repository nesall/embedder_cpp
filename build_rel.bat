echo Building embedder_cpp release version...
mkdir build_rel
cd build_rel
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ..

echo Copying release artifacts to dist folder...
rm dist -rf
rm dist.zip -f
mkdir dist
xcopy build_rel\Release\* dist\ /E /Y
xcopy build_rel\public dist\public\ /E /I
rm dist\output.log -f
rm dist\diagnostics.log -f
rm dist\sqlite3_lib.lib -f
xcopy assets\README dist\
xcopy assets\settings.template.json dist\
xcopy assets\bge_tokenizer.json dist\
xcopy scripts\install-service.bat dist\
xcopy scripts\uninstall-service.bat dist\
xcopy scripts\start.bat dist\
xcopy scripts\stop.bat dist\


echo Creating dist.zip...
powershell -NoProfile -Command "Compress-Archive -Path 'dist\*' -DestinationPath 'dist.zip' -Force"

echo Build complete. Package is in dist folder!