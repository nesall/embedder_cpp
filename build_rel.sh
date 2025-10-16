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
mkdir -p dist
cp -r build_rel/* dist/
cp settings.template.json dist/
cp bge_tokenizer.json dist/

echo "Build complete. Package is in dist folder!"
