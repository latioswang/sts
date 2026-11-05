#!/usr/bin/env python3
"""Writes the game profile the simulator assumes: everything unlocked, no first-run behaviour.

Usage: write_profile.py <preferences dir>

A fresh profile differs from the simulator in ways that have nothing to do with RNG. Locked cards and relics
change the reward pools, the first run has no Neow (NEOW_SKIP in Exordium's constructor), unseen bosses are forced, and
the final act is off until three characters have won. Keys and values come from the game's bytecode. Other keys in an
existing file are kept.
"""
import json
import sys
from pathlib import Path

UNLOCKED = "2"

LOCKED_CARDS = (
    "Havoc", "Sentinel", "Exhume", "Wild Strike", "Evolve", "Immolate", "Heavy Blade", "Spot Weakness", "Limit Break",
    "Concentrate", "Setup", "Grand Finale", "Cloak And Dagger", "Accuracy", "Storm of Steel", "Bane", "Catalyst",
    "Corpse Explosion", "Rebound", "Undo", "Echo Form", "Turbo", "Sunder", "Meteor Strike", "Hyperbeam", "Recycle",
    "Core Surge", "Prostrate", "Blasphemy", "Devotion", "ForeignInfluence", "Alpha", "MentalFortress", "SpiritShield",
    "Wish", "Wireheading",
)
LOCKED_RELICS = (
    "Omamori", "Prayer Wheel", "Shovel", "Art of War", "The Courier", "Pandora's Box", "Blue Candle", "Dead Branch",
    "Singing Bowl", "Du-Vu Doll", "Smiling Mask", "Tiny Chest", "Cables", "DataDisk", "Emotion Chip",
    "Runic Capacitor", "Turnip", "Symbiotic Virus", "Akabeko", "Yang", "CeramicFish", "StrikeDummy", "TeardropLocket",
    "CloakClasp",
)
LOCKED_CHARACTERS = ("The Silent", "Defect", "Watcher")
CHARACTER_IDS = ("IRONCLAD", "THE_SILENT", "DEFECT", "WATCHER")
MAX_UNLOCK_LEVEL = "5"
# Until a boss has been seen, initializeBoss forces it instead of rolling the seed's shuffle.
BOSSES = ("GUARDIAN", "GHOST", "SLIME", "CHAMP", "AUTOMATON", "COLLECTOR", "CROW", "DONUT", "WIZARD")

TIPS = (
    "NEOW_SKIP", "NEOW_INTRO", "NO_FTUE", "COMBAT_TIP", "BLOCK TIP", "POWER_TIP", "M_POWER_TIP", "ENERGY_USE_TIP",
    "SHUFFLE_TIP", "POTION_TIP", "CARD_REWARD_TIP", "INTENT_TIP", "RELIC_TIP",
)


def profile() -> dict[str, dict[str, str]]:
    return {
        "STSUnlocks": {key: UNLOCKED for key in LOCKED_CARDS + LOCKED_RELICS + LOCKED_CHARACTERS},
        "STSUnlockProgress": {f"{c}UnlockLevel": MAX_UNLOCK_LEVEL for c in CHARACTER_IDS},
        "STSSeenBosses": {boss: "1" for boss in BOSSES},
        # Settings.setFinalActAvailability: keys, burning elites and act 4 need a win with these three characters.
        "STSPlayer": {f"{c}_WIN": "true" for c in ("IRONCLAD", "THE_SILENT", "DEFECT")},
        # isNoteForYourselfAvailable: from A1 to A14 it needs an ascension above the run's, which the simulator assumes.
        **{f"STSData{c}": {"ASCENSION_LEVEL": "20"} for c in ("Vagabond", "TheSilent", "Defect", "Watcher")},
        "STSTips": {tip: "true" for tip in TIPS},
        # Fast Mode: Communication Mod is only tested with it. Upload Data: research runs must not reach the devs' stats.
        "STSGameplaySettings": {"Fast Mode": "true", "Upload Data": "false"},
        # Batches and live runs play for hours, so the game stays silent.
        "STSSound": {"Master Volume": "0.0", "Music Volume": "0.0", "Sound Volume": "0.0"},
        # The profile name stops the game opening the save slot screen at the main menu.
        "STSSaveSlots": {"DEFAULT_SLOT": "0", "PROFILE_NAME": "Bridge"},
    }


def read_existing(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    for name, values in profile().items():
        path = directory / name
        merged = {**read_existing(path), **values}
        path.write_text(json.dumps(merged, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {path} ({len(values)} keys)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
