#!/usr/bin/env bash
set -e

echo "Building phenixcode-core release version..."
mkdir -p build_rel/out
cd build_rel
cmake -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=out -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
cd ..

# Debug: Check what was actually built
echo "Searching for phenixcode-core binary:"
find build_rel -name "phenixcode-core" -type f

echo "Contents of build_rel:"
ls -la build_rel/

echo "Contents of build_rel/out:"
ls -la build_rel/out/

echo "Copying release artifacts to dist/..."
rm -rf dist
mkdir -p dist

# Copy the binary if it exists in out directory
if [ -f "build_rel/out/phenixcode-core" ]; then
    cp "build_rel/out/phenixcode-core" dist/
    echo "Successfully copied phenixcode-core to dist/"
else
    echo "ERROR: phenixcode-core not found in build_rel/out/"
    # Try to find it elsewhere as fallback
    BINARY_PATH=$(find build_rel -name "phenixcode-core" -type f | head -1)
    if [ -n "$BINARY_PATH" ]; then
        echo "Found binary at: $BINARY_PATH"
        cp "$BINARY_PATH" dist/
        echo "Copied from alternative location"
    else
        echo "ERROR: Could not find phenixcode-core binary anywhere!"
        exit 1
    fi
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