#!/usr/bin/env bash
set -e

echo "Building phenixcode-core release version..."
mkdir -p build_rel/out
cd build_rel
cmake -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=out -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
cd ..

echo "Debug: Checking for built executable..."
find . -name "phenixcode-core" -type f | head -5

echo "Copying release artifacts to dist folder..."
rm -rf dist
rm -f dist.zip
mkdir -p dist

# Copy the executable (using find to locate it)
find build_rel -name "phenixcode-core" -type f -exec cp {} dist/ \;

# Continue with other files
cp -r build_rel/public dist/ 2>/dev/null || echo "public folder not found, skipping"
rm -f dist/output.log
rm -f dist/diagnostics.log
cp assets/README dist/
cp assets/settings.template.json dist/
cp assets/bge_tokenizer.json dist/
cp scripts/install-service.sh dist/
cp scripts/uninstall-service.sh dist/
cp scripts/start.sh dist/
cp scripts/stop.sh dist/

echo "Build complete. Package is in dist folder!"