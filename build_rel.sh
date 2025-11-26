#!/usr/bin/env bash
set -e

echo "Building phenixcode-core release version..."
echo "Trying CMake install approach..."
mkdir -p build_rel
cd build_rel
cmake -DCMAKE_INSTALL_PREFIX=../dist -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
echo "=== BEFORE INSTALL ==="
find . -name "phenixcode-core" -type f
cmake --install . --config Release
echo "=== AFTER INSTALL ==="
cd ..
find . -name "phenixcode-core" -type f

echo "Final dist contents:"
ls -la dist/

# Continue with other files
echo "Deleting .log files from dist/ if any..."
rm -f dist/output.log
rm -f dist/diagnostics.log
echo "Copying scripts and setup files to dist/ ..."
cp -r build_rel/public dist/ 2>/dev/null || echo "public folder not found, skipping"
cp assets/README dist/
cp assets/settings.template.json dist/
cp assets/bge_tokenizer.json dist/
cp scripts/install-service.sh dist/
cp scripts/uninstall-service.sh dist/
cp scripts/start.sh dist/
cp scripts/stop.sh dist/

echo "Build complete. Package is in dist folder!"