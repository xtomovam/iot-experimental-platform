#!/bin/bash

REPO_ROOT="$(dirname "$0")/.."
CONFIG_FILE="$REPO_ROOT/config/config.toml"
GENERATOR="$REPO_ROOT/config/generate_config.py"

EDITOR_CMD="code --wait"

echo "Opening configuration file..."
$EDITOR_CMD "$CONFIG_FILE"

echo "Generating updated configs..."
python3 "$GENERATOR"

echo "Done!"
