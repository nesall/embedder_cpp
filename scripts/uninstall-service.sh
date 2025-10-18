#!/usr/bin/env bash
# uninstall-service.sh
# Usage: sudo ./uninstall-service.sh
set -euo pipefail

### CONFIG - keep consistent with install script
SERVICE_NAME="embedder-rag-core"
SERVICE_USER="embedder"
INSTALL_DIR="/opt/${SERVICE_NAME}"
UNIT_PATH="/etc/systemd/system/${SERVICE_NAME}.service"
LOG_DIR="/var/log/${SERVICE_NAME}"
### END CONFIG

if [[ $EUID -ne 0 ]]; then
  echo "Run as root (sudo)."
  exit 1
fi

if ! command -v systemctl >/dev/null 2>&1; then
  echo "systemd (systemctl) not found on this machine. Aborting."
  exit 1
fi

echo "Stopping service if running..."
if systemctl is-active --quiet "${SERVICE_NAME}.service"; then
  systemctl stop "${SERVICE_NAME}.service" || true
fi

echo "Disabling service..."
if systemctl is-enabled --quiet "${SERVICE_NAME}.service"; then
  systemctl disable "${SERVICE_NAME}.service" || true
fi

echo "Removing systemd unit..."
if [[ -f "$UNIT_PATH" ]]; then
  rm -f "$UNIT_PATH"
  systemctl daemon-reload
fi

echo "Removing installed files (if present)..."
if [[ -d "$INSTALL_DIR" ]]; then
  rm -rf "$INSTALL_DIR"
fi

echo "Removing logs (if present)..."
if [[ -d "$LOG_DIR" ]]; then
  rm -rf "$LOG_DIR"
fi

echo "Optionally removing service user: $SERVICE_USER (will only remove if system user exists and has no processes)"
if id -u "$SERVICE_USER" >/dev/null 2>&1; then
  # only remove the user if it is a system user and has no running processes
  pkill -u "$SERVICE_USER" || true
  userdel "$SERVICE_USER" 2>/dev/null || echo "User $SERVICE_USER not removed (may have processes or be a non-system account)."
fi

echo "Uninstall complete. Check: sudo systemctl status ${SERVICE_NAME}.service"
