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

cd "$RUN_DIR"
exec arch -x86_64 jre/bin/java -Xmx3G -jar "$MTS" --skip-launcher --skip-intro --mods basemod,CommunicationMod
