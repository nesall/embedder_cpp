#!/usr/bin/env bash
# stop.sh
PID_FILE="process.pid"

if [[ ! -f "$PID_FILE" ]]; then
    echo "PID file not found. Is the service running?"
    exit 1
fi

pid=$(cat "$PID_FILE")
if ! kill -0 $pid 2>/dev/null; then
    echo "Process $pid not running."
    rm -f "$PID_FILE"
    exit 0
fi

echo "Stopping Embedder RAG (PID $pid)..."
kill $pid           # SIGTERM, allows graceful shutdown
sleep 2
if kill -0 $pid 2>/dev/null; then
    echo "Process did not stop, forcing SIGKILL..."
    kill -9 $pid
fi

rm -f "$PID_FILE"
echo "Stopped."
