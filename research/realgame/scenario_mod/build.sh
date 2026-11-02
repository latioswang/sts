#!/usr/bin/env bash
# Builds the test-only scenario mod into the game folder's mods/ (or the folder given as $1). Needs a JDK 8:
# set JAVA8_HOME if javac on the path is a different version. launch_game.sh loads the mod when the jar is there.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="${STS_GAME_DIR:-$HOME/.sts_realgame}"
OUT_DIR="${1:-$GAME/mods}"
JAVAC="${JAVA8_HOME:+$JAVA8_HOME/bin/}javac"
JAR="${JAVA8_HOME:+$JAVA8_HOME/bin/}jar"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
"$JAVAC" -source 8 -target 8 -nowarn -d "$WORK" \
  -cp "$GAME/desktop-1.0.jar:$GAME/ModTheSpire.jar:$GAME/mods/BaseMod.jar" \
  "$HERE"/src/bridgescenario/*.java
cp "$HERE/ModTheSpire.json" "$WORK/"
"$JAR" cf "$OUT_DIR/BridgeScenario.jar" -C "$WORK" .
echo "built $OUT_DIR/BridgeScenario.jar"
