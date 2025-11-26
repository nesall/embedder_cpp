#!/usr/bin/env bash
set -e

echo "=== BUILD WITH PROPER ERROR HANDLING ==="
mkdir -p build_rel
cd build_rel

# Clean previous build
rm -rf *

# Configure with more verbose output
cmake -DCMAKE_BUILD_TYPE=Release .. 2>&1 | tee cmake.log
if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo "CMAKE CONFIGURATION FAILED"
    exit 1
fi

# Build with detailed output and proper error checking
echo "=== STARTING BUILD ==="
make phenixcode-core VERBOSE=1 2>&1 | tee make.log
BUILD_EXIT_CODE=${PIPESTATUS[0]}

echo "=== BUILD EXIT CODE: $BUILD_EXIT_CODE ==="

# Check if build actually succeeded
if [ $BUILD_EXIT_CODE -eq 0 ]; then
    echo "=== BUILD SUCCEEDED - SEARCHING FOR BINARY ==="
    find . -name "phenixcode-core" -type f -ls 2>/dev/null
    find . -type f -executable -ls 2>/dev/null | head -10
    
    echo "=== CURRENT DIRECTORY CONTENTS ==="
    ls -la
    
    echo "=== ATTEMPTING TO RUN BINARY ==="
    ./phenixcode-core --version 2>&1 || echo "Binary cannot be executed"
else
    echo "=== BUILD FAILED - CHECKING ERRORS ==="
    grep -i "error\|fail\|undefined\|cannot" make.log | head -20
fi

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