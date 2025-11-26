#!/usr/bin/env bash
set -e

echo "Building phenixcode-core release version..."
mkdir -p build_rel
cd build_rel
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
cd ..

echo "=== SEARCHING FOR BINARY ==="
# The binary should be in the build_rel directory itself
find build_rel -name "phenixcode-core" -type f

echo "=== CHECKING BUILD_REL DIRECTORY ==="
ls -la build_rel/ | grep phenixcode

echo "=== COPYING BINARY ==="
rm -rf dist
mkdir -p dist

# The binary should be directly in build_rel/
if [ -f "build_rel/phenixcode-core" ]; then
    echo "Found binary at: build_rel/phenixcode-core"
    cp "build_rel/phenixcode-core" dist/
    cp -r build_rel/public/ dist/public/ 2>/dev/null || true
    cp README.md settings.json dist/ 2>/dev/null || true
    echo "Successfully copied phenixcode-core to dist/"
else
    echo "ERROR: phenixcode-core not found in build_rel/"
    echo "Available files in build_rel/:"
    ls -la build_rel/
    exit 1
fi

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