#!/bin/bash

# Exit on any error
set -e

APP_NAME="CodeKeeper"
INSTALL_DIR="/usr/local/bin"
#DATA_DIR="/var/lib/CodeKeeperRepo"
BINARY_NAME="codekeeper"  
SOURCE_BINARY="./build/$BINARY_NAME"  

echo "🔧 Installing $APP_NAME..."

# Check if the binary exists
if [ ! -f "$SOURCE_BINARY" ]; then
    echo "❌ Binary not found at: $SOURCE_BINARY"
    exit 1
fi

# Install the binary
echo "📥 Copying binary to $INSTALL_DIR..."
sudo cp "$SOURCE_BINARY" "$INSTALL_DIR/"
sudo chmod +x "$INSTALL_DIR/$BINARY_NAME"

# Create data directory
# echo "📁 Creating data directory at $DATA_DIR..."
# sudo mkdir -p "$DATA_DIR"
# sudo chown "$USER":"$USER" "$DATA_DIR"
# sudo chmod 755 "$DATA_DIR"

echo "✅ $APP_NAME installed successfully!"
echo "📂 Repo directory: $DATA_DIR"
echo "🚀 Run it with: $BINARY_NAME"
