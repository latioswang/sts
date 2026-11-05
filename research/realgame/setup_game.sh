#!/usr/bin/env bash
# Builds the bridge's own game folder and points Communication Mod at commrelay.py. Safe to re-run while the game is
# closed. See README.md.
#
# The game keeps preferences, saves and run history in its working directory. A separate folder keeps the full-unlock
# profile and the research runs out of the Steam install, where Steam Cloud could sync them.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
STEAMAPPS="$HOME/Library/Application Support/Steam/steamapps"
GAME="$STEAMAPPS/common/SlayTheSpire/SlayTheSpire.app/Contents/Resources"
WORKSHOP="$STEAMAPPS/workshop/content/646570"
RUN_DIR="${STS_GAME_DIR:-$HOME/.sts_realgame}"
PYTHON="${STS_PYTHON:-$(command -v python3)}"

for required in "$GAME/desktop-1.0.jar" "$GAME/jre" "$WORKSHOP/1605060445/ModTheSpire.jar" \
                "$WORKSHOP/1605833019/BaseMod.jar" "$WORKSHOP/2131373661/CommunicationMod.jar"; do
  if [ ! -e "$required" ]; then
    echo "error: not found: $required" >&2
    echo "Install Slay the Spire and the three Workshop mods. See README.md." >&2
    exit 1
  fi
done

mkdir -p "$RUN_DIR/mods"
ln -sfn "$GAME/desktop-1.0.jar" "$RUN_DIR/desktop-1.0.jar"
ln -sfn "$GAME/jre" "$RUN_DIR/jre"
# ModTheSpire's Workshop search needs a working Steam API, which it doesn't have here (see README.md). mods/ always works.
cp "$WORKSHOP/1605060445/ModTheSpire.jar" "$RUN_DIR/ModTheSpire.jar"
cp "$WORKSHOP/1605833019/BaseMod.jar" "$WORKSHOP/2131373661/CommunicationMod.jar" "$RUN_DIR/mods/"
echo "game folder: $RUN_DIR"

"$PYTHON" "$HERE/write_profile.py" "$RUN_DIR/preferences"
# Width, height, frame limit, fullscreen, windowed fullscreen, vsync. With vsync on, the game drops to a few frames per
# second once the display sleeps or the window is covered.
printf '1280\n720\n240\nfalse\nfalse\nfalse\n' > "$RUN_DIR/info.displayconfig"

CONFIG_DIR="$HOME/Library/Preferences/ModTheSpire/CommunicationMod"
CONFIG="$CONFIG_DIR/config.properties"
# The mod splits this command on whitespace, so neither path may contain a space.
COMMAND="$PYTHON $HERE/commrelay.py"
case "$COMMAND" in
  *" "*" "*) echo "error: a path in '$COMMAND' contains a space; the mod cannot launch it" >&2; exit 1 ;;
esac

mkdir -p "$CONFIG_DIR"
cat > "$CONFIG" <<EOF
command=$COMMAND
runAtGameStart=true
maxInitializationTimeout=10
verbose=false
EOF
echo "wrote $CONFIG"
cat "$CONFIG"
