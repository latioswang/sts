#!/usr/bin/env bash
# Starts Slay the Spire with ModTheSpire, BaseMod and Communication Mod from the folder setup_game.sh built, skipping
# the mod launcher window. The game's own Java 8 runtime is x86_64, so this needs Rosetta on Apple silicon.
set -euo pipefail
RUN_DIR="${STS_GAME_DIR:-$HOME/.sts_realgame}"
MTS="${STS_MTS_JAR:-$RUN_DIR/ModTheSpire.jar}"

for required in "$RUN_DIR/desktop-1.0.jar" "$RUN_DIR/jre/bin/java" "$MTS" "$RUN_DIR/mods/BaseMod.jar" \
                "$RUN_DIR/mods/CommunicationMod.jar"; do
  if [ ! -e "$required" ]; then
    echo "error: not found: $required" >&2
    echo "Run setup_game.sh first. See README.md." >&2
    exit 1
  fi
done

MODS=basemod,CommunicationMod
# The test-only scenario mod, if it has been built. It does nothing until a mirror run asks it to.
if [ -e "$RUN_DIR/mods/BridgeScenario.jar" ]; then
  MODS="$MODS,bridgescenario"
fi

# A run's end writes the character's file back, so the profile goes on again at each start.
python3 -I "$(dirname "$0")/write_profile.py" "$RUN_DIR/preferences" > /dev/null

# BaseMod's fixes change the game itself (an orb slot on a first channel, reward culling and more), so the game runs
# without them, as an unmodded one would.
BASEMOD_CONFIG="${STS_BASEMOD_CONFIG:-$HOME/Library/Preferences/ModTheSpire/BaseMod/basemod-config.properties}"
mkdir -p "$(dirname "$BASEMOD_CONFIG")"
touch "$BASEMOD_CONFIG"
{ grep -v '^basemod-fixes=' "$BASEMOD_CONFIG" || true; echo "basemod-fixes=false"; } > "$BASEMOD_CONFIG.tmp"
mv "$BASEMOD_CONFIG.tmp" "$BASEMOD_CONFIG"

cd "$RUN_DIR"
exec arch -x86_64 jre/bin/java -Xmx3G -jar "$MTS" --skip-launcher --skip-intro --mods "$MODS"
