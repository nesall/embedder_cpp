#!/usr/bin/env bash
set -e

if [[ -z "$1" ]]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 1.0.1"
    exit 1
fi

VER="$1"
NAME="phenixcode-v${VER}-linux-x64"

echo "VER=$VER"
echo "NAME=$NAME"

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
