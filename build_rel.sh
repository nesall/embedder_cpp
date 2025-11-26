#!/usr/bin/env bash
set -e

echo "=== COMPREHENSIVE DEBUGGING ==="
echo "Building phenixcode-core release version..."
mkdir -p build_rel
cd build_rel
cmake -DCMAKE_BUILD_TYPE=Release ..

echo "=== BUILD WITH ERROR CHECKING ==="

# Run build and capture all output
set -x
cmake --build . --config Release --parallel 2>&1 | tee build.log
set +x

echo "=== CHECKING BUILD LOG ==="
grep -i "error\|fail\|cannot\|no such" build.log || echo "No obvious errors in build log"

# Check the actual linking command
echo "=== CHECKING LINK COMMAND ==="
grep -A2 -B2 "Linking.*phenixcode-core" build.log

cd ..

echo "=== EXTENSIVE SEARCH ==="
echo "1. Searching for ANY executable files in build_rel:"
find build_rel -type f -executable -ls 2>/dev/null

echo "2. Searching for files containing 'phenixcode':"
find build_rel -type f -name "*phenixcode*" 2>/dev/null

echo "3. Checking build_rel directory structure:"
ls -la build_rel/

echo "4. Checking if file exists at exact path:"
[ -f "build_rel/phenixcode-core" ] && echo "FOUND: build_rel/phenixcode-core" || echo "NOT FOUND: build_rel/phenixcode-core"

echo "5. Checking file creation during build:"
cd build_rel
echo "Running make with dry-run to see what would be executed:"
make -n | grep -A5 -B5 "phenixcode-core" || echo "No phenixcode-core target found in dry-run"
cd ..

echo "6. Checking build system type:"
cd build_rel
ls -la CMakeFiles/ | grep phenixcode
cd ..

echo "=== BUILD REL CONTENTS RECURSIVE (first level) ==="
find build_rel -maxdepth 2 -type f -ls 2>/dev/null | head -20


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