#!/usr/bin/env bash
set -euo pipefail

# --- Parse version.cmake ---
MAJOR=$(grep '^set(EMBEDDER_VERSION_MAJOR' version.cmake | sed -E 's/.* ([0-9]+).*/\1/')
MINOR=$(grep '^set(EMBEDDER_VERSION_MINOR' version.cmake | sed -E 's/.* ([0-9]+).*/\1/')
PATCH=$(grep '^set(EMBEDDER_VERSION_PATCH' version.cmake | sed -E 's/.* ([0-9]+).*/\1/')

VER="${MAJOR}.${MINOR}.${PATCH}"
NAME="phenixcode-v${VER}-linux-x64"

EMBEDDER="./"
CLIENT="ui/clients/webview"
DASHBOARD="ui/dashboard/webview"

cd "$EMBEDDER"
./build_rel.sh
echo "FINISHED $EMBEDDER"

cd "$CLIENT"
./build_rel.sh
cd ../../..

cd "$DASHBOARD"
./build_rel.sh
cd ../../..

rm -rf "$NAME"
mkdir "$NAME"

cp -r "$EMBEDDER/dist/"* "$NAME/"
cp -r "$CLIENT/dist/"* "$NAME/"
cp -r "$DASHBOARD/dist/"* "$NAME/"

echo "$NAME.zip..."
rm -f "$NAME.zip"
zip -r "$NAME.zip" "$NAME" >/dev/null

rm -rf "$NAME"

echo "Package '$NAME.zip' ready."
