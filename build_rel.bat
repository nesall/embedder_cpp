echo Building phenixcode-core release version...
mkdir build_rel
cd build_rel
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ..

set DIRNAME=dist

echo Copying release artifacts to %DIRNAME% folder...
rm %DIRNAME% -rf
rm %DIRNAME%.zip -f
mkdir %DIRNAME%
xcopy build_rel\Release\* %DIRNAME%\ /E /Y
xcopy build_rel\public %DIRNAME%\public\ /E /I
rm %DIRNAME%\output.log -f
rm %DIRNAME%\diagnostics.log -f
rm %DIRNAME%\sqlite3_lib.lib -f
xcopy assets\README %DIRNAME%\
xcopy assets\settings.template.json %DIRNAME%\
xcopy assets\bge_tokenizer.json %DIRNAME%\
xcopy scripts\install-service.bat %DIRNAME%\
xcopy scripts\uninstall-service.bat %DIRNAME%\
xcopy scripts\start.bat %DIRNAME%\
xcopy scripts\stop.bat %DIRNAME%\


echo Creating %DIRNAME%.zip...
powershell -NoProfile -Command "Compress-Archive -Path '%DIRNAME%\*' -DestinationPath '%DIRNAME%.zip' -Force"

echo Build complete. Package is in %DIRNAME% folder!