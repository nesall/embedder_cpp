#!/bin/bash
echo "Building RAG Code Assistant WebView..."

# Build the webview
echo "Building webview..."
cd ../webview
mkdir -p build
cd build
cmake ..
make -j$(nproc)

echo "Build complete! Run with: ./rag-code-assistant-webview"

