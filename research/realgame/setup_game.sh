#!/usr/bin/env bash
# Points Communication Mod at commrelay.py. Safe to re-run. See README.md.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CONFIG_DIR="$HOME/Library/Preferences/ModTheSpire/CommunicationMod"
CONFIG="$CONFIG_DIR/config.properties"
# The mod splits this command on whitespace, so neither path may contain a space.
PYTHON="${STS_PYTHON:-$(command -v python3)}"
COMMAND="$PYTHON $HERE/commrelay.py"
case "$COMMAND" in
  *" "*" "*) echo "error: a path in '$COMMAND' contains a space; the mod cannot launch it" >&2; exit 1 ;;
esac

mkdir -p "$CONFIG_DIR"
cat > "$CONFIG" <<EOF
command=$COMMAND
runAtGameStart=true
maxInitializationTimeout=10
verbose=true
EOF
echo "wrote $CONFIG"
cat "$CONFIG"
