#!/usr/bin/env bash
set -e

echo "Building phenixcode-core release version..."
mkdir -p build_rel
cd build_rel
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
cd ..

echo "=== DEBUGGING BINARY LOCATION ==="
echo "Searching recursively for phenixcode-core:"
find build_rel -name "phenixcode-core" -type f 2>/dev/null

echo "Current directory: $(pwd)"
echo "Listing build_rel contents recursively:"
find build_rel -type f -name "*phenixcode*" 2>/dev/null

echo "Checking if binary exists in common locations:"
[ -f "build_rel/phenixcode-core" ] && echo "Found at: build_rel/phenixcode-core"
[ -f "build_rel/out/phenixcode-core" ] && echo "Found at: build_rel/out/phenixcode-core"
[ -f "build_rel/src/phenixcode-core" ] && echo "Found at: build_rel/src/phenixcode-core"

# Copy whatever we find
BINARY_PATH=$(find build_rel -name "phenixcode-core" -type f | head -1)
if [ -n "$BINARY_PATH" ]; then
    echo "Found binary at: $BINARY_PATH"
    rm -rf dist
    mkdir -p dist
    cp "$BINARY_PATH" dist/
    echo "Successfully copied to dist/"
    ls -la dist/
else
    echo "ERROR: Could not find phenixcode-core binary"
    exit 1
fi

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