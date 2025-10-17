echo Building embedder_cpp release version...
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
rm dist\sqlite3_lib.lib -f
xcopy settings.template.json dist\
xcopy bge_tokenizer.json dist\

echo Build complete. Package is in dist folder!