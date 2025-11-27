#!/usr/bin/env bash
# install-service.sh
# Usage: sudo ./install-service.sh
set -euo pipefail

### CONFIG - edit as needed
SERVICE_NAME="phenixcode-rag-core" # systemd unit name (without .service)
SERVICE_USER="embedder" # system user that will run the service
INSTALL_DIR="/opt/${SERVICE_NAME}" # where binary and assets will be installed
LOG_DIR="/var/log/${SERVICE_NAME}"
BINARY_NAME="phenixcode-core" # change to your Linux binary name
BINARY_SOURCE="$(pwd)/${BINARY_NAME}" # expected path where user runs this script
BINARY_ARGS="serve --port 8081 --watch 60" # default args
UNIT_PATH="/etc/systemd/system/${SERVICE_NAME}.service"
### END CONFIG

if [[ $EUID -ne 0 ]]; then
  echo "This installer must be run as root (sudo)."
  exit 1
fi

# check systemd
if ! command -v systemctl >/dev/null 2>&1; then
  echo "systemd (systemctl) not found on this machine. Aborting."
  exit 1
fi

# check binary present
if [[ ! -x "$BINARY_SOURCE" ]]; then
  echo "Binary not found or not executable: $BINARY_SOURCE"
  echo "Place your built binary at that path or update BINARY_SOURCE in the script."
  exit 1
fi

# create system user if not exists
if ! id -u "$SERVICE_USER" >/dev/null 2>&1; then
  echo "Creating system user: $SERVICE_USER"
  useradd --system --no-create-home --shell /usr/sbin/nologin "$SERVICE_USER"
fi

# create dirs
mkdir -p "$INSTALL_DIR"
mkdir -p "$LOG_DIR"
chown root:"$SERVICE_USER" "$INSTALL_DIR"
chown "$SERVICE_USER":"$SERVICE_USER" "$LOG_DIR"

# copy binary
cp -f "$BINARY_SOURCE" "$INSTALL_DIR/$BINARY_NAME"
chmod 0755 "$INSTALL_DIR/$BINARY_NAME"
chown root:"$SERVICE_USER" "$INSTALL_DIR/$BINARY_NAME"

# create minimal working dir for runtime if needed
mkdir -p "$INSTALL_DIR/data"
chown "$SERVICE_USER":"$SERVICE_USER" "$INSTALL_DIR/data"

# write systemd unit
cat > "$UNIT_PATH" <<EOF
[Unit]
Description=Embedder RAG Service
After=network.target

[Service]
Type=simple
User=${SERVICE_USER}
Group=${SERVICE_USER}
WorkingDirectory=${INSTALL_DIR}
ExecStart=${INSTALL_DIR}/${BINARY_NAME} ${BINARY_ARGS}
Restart=on-failure
RestartSec=5
Environment=NODE_ENV=production

[Install]
WantedBy=multi-user.target
EOF

# reload, enable and start
systemctl daemon-reload
systemctl enable --now "${SERVICE_NAME}.service"

echo "Service '${SERVICE_NAME}' installed and started."
echo "To view status: sudo systemctl status ${SERVICE_NAME}.service"
