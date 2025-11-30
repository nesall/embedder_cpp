#!/usr/bin/env bash
set -e

VER="${1:-1.0.2}"
NAME="phenixcode-v${VER}-macos-arm64"

echo "VER=$VER"
echo "NAME=$NAME"

EMBEDDER="./"
WEBVIEW="ui/clients/webview"

cd "$EMBEDDER"
./build_rel.sh
echo "FINISHED $EMBEDDER"

cd "$WEBVIEW"
./build_rel.sh
cd ../../..

rm -rf "$NAME"
mkdir "$NAME"

cp -r "$EMBEDDER/dist/"* "$NAME/"
cp -r "$WEBVIEW/dist/"* "$NAME/"

echo "$NAME.zip..."
rm -f "$NAME.zip"
zip -r "$NAME.zip" "$NAME" >/dev/null

rm -rf "$NAME"

echo "Package '$NAME.zip' ready."
