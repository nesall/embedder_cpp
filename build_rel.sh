#!/usr/bin/env bash
set -e

echo "Building embedder_cpp release version..."
mkdir -p build_rel
cd build_rel
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ..

echo "Copying release artifacts to dist folder..."
rm -rf dist
rm -f dist.zip
mkdir -p dist
cp -r build_rel/Release/* dist/
cp -r build_rel/public dist/
rm dist\output.log -f
rm dist\diagnostics.log -f
cp assets/README dist/
cp assets/settings.template.json dist/
cp assets/bge_tokenizer.json dist/
cp scripts/install-service.sh dist/
cp scripts/uninstall-service.sh dist/
cp scripts/start.sh dist/
cp scripts/stop.sh dist/

echo "Build complete. Package is in dist folder!"
