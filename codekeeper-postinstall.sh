#!/bin/bash
# CodeKeeper post-installation setup script
# This script should be run as root after copying codekeeper to /usr/local/bin

set -e

# Create central storage for all repositories
CENTRAL_DIR="/var/lib/CodeKeeper"

if [ ! -d "$CENTRAL_DIR" ]; then
    mkdir -p "$CENTRAL_DIR"
    echo "Created $CENTRAL_DIR"
fi

# Set permissions so all users can create their own repos
chmod 1777 "$CENTRAL_DIR"

# Optionally, create a skeleton for new repos (for reference)
REPO_SKELETON="$CENTRAL_DIR/.repo_skeleton"
if [ ! -d "$REPO_SKELETON" ]; then
    mkdir -p "$REPO_SKELETON/versions"
    touch "$REPO_SKELETON/.bypass"
    touch "$REPO_SKELETON/commit_log.txt"
    touch "$REPO_SKELETON/.users"
    touch "$REPO_SKELETON/.remote"
    touch "$REPO_SKELETON/.sessions"
    echo "# Add files or patterns to ignore" > "$REPO_SKELETON/.bypass"
    echo "Created skeleton repo structure at $REPO_SKELETON"
fi

# Print success message
echo "CodeKeeper system directories initialized."
echo "You can now use 'codekeeper init <project>' to create a new repository."
