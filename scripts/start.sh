#!/usr/bin/env bash
# start.sh

PID_FILE="process.pid"
BINARY="./embeddings_cpp"
ARGS="serve --port 8590 --watch --interval 60"

if [[ -f "$PID_FILE" ]] && kill -0 $(cat "$PID_FILE") 2>/dev/null; then
    echo "Service already running (PID $(cat $PID_FILE))"
    exit 1
fi

echo "Starting Embedder RAG manually..."
$BINARY $ARGS &

pid=$!
echo $pid > "$PID_FILE"
echo "Started with PID = $pid"
