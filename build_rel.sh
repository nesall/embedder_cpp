#!/usr/bin/env bash
set -e

echo "=== INVESTIGATING LINKING ISSUE ==="
cd build_rel

# Check the actual link command that CMake generated
echo "=== LINK.TXT CONTENTS ==="
cat CMakeFiles/phenixcode-core.dir/link.txt

# Check if there are any object files
echo "=== OBJECT FILES ==="
ls -la CMakeFiles/phenixcode-core.dir/src/*.o 2>/dev/null | wc -l

# Try running the link command manually
echo "=== MANUAL LINK ATTEMPT ==="
LINK_CMD=$(cat CMakeFiles/phenixcode-core.dir/link.txt)
echo "Link command length: ${#LINK_CMD}"
echo "First 500 chars of link command:"
echo "${LINK_CMD:0:500}"
echo "..."
echo "Last 500 chars of link command:"
echo "${LINK_CMD: -500}"

# Try to execute the link command
echo "=== EXECUTING LINK COMMAND ==="
eval "$LINK_CMD" 2>&1 | tee manual_link.log

if [ -f "phenixcode-core" ]; then
    echo "MANUAL LINK SUCCESS!"
    ls -la phenixcode-core
else
    echo "MANUAL LINK FAILED"
    echo "=== LINK ERRORS ==="
    grep -i "error\|fail\|undefined" manual_link.log
fi

cd ..
echo "6. Checking build system type:"
cd build_rel
ls -la CMakeFiles/ | grep phenixcode
cd ..

echo "=== BUILD REL CONTENTS RECURSIVE (first level) ==="
find build_rel -maxdepth 2 -type f -ls 2>/dev/null | head -20


mkdir -p dist/
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