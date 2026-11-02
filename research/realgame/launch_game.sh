#!/usr/bin/env bash
# Starts Slay the Spire with ModTheSpire, BaseMod and Communication Mod, skipping the mod launcher window.
# Steam must be running. The game's own Java 8 runtime is x86_64, so this needs Rosetta on Apple silicon.
set -euo pipefail
STEAMAPPS="$HOME/Library/Application Support/Steam/steamapps"
GAME="$STEAMAPPS/common/SlayTheSpire/SlayTheSpire.app/Contents/Resources"
WORKSHOP="$STEAMAPPS/workshop/content/646570"
MTS="${STS_MTS_JAR:-$WORKSHOP/1605060445/ModTheSpire.jar}"

for required in "$GAME/desktop-1.0.jar" "$MTS"; do
  if [ ! -f "$required" ]; then
    echo "error: not found: $required" >&2
    echo "Install Slay the Spire and subscribe to ModTheSpire, BaseMod and Communication Mod. See README.md." >&2
    exit 1
  fi
done

cd "$GAME"
exec arch -x86_64 jre/bin/java -Xmx1G -jar "$MTS" --skip-launcher --skip-intro --mods basemod,CommunicationMod
